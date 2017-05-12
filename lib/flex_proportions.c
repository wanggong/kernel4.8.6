/*
 *  Floating proportions with flexible aging period
 *
 *   Copyright (C) 2011, SUSE, Jan Kara <jack@suse.cz>
 *
 * The goal of this code is: Given different types of event, measure proportion
 * of each type of event over time. The proportions are measured with
 * exponentially decaying history to give smooth transitions. A formula
 * expressing proportion of event of type 'j' is:
 *
 *   p_{j} = (\Sum_{i>=0} x_{i,j}/2^{i+1})/(\Sum_{i>=0} x_i/2^{i+1})
 *
 * Where x_{i,j} is j's number of events in i-th last time period and x_i is
 * total number of events in i-th last time period.
 *
 * Note that p_{j}'s are normalised, i.e.
 *
 *   \Sum_{j} p_{j} = 1,
 *
 * This formula can be straightforwardly computed by maintaining denominator
 * (let's call it 'd') and for each event type its numerator (let's call it
 * 'n_j'). When an event of type 'j' happens, we simply need to do:
 *   n_j++; d++;
 *
 * When a new period is declared, we could do:
 *   d /= 2
 *   for each j
 *     n_j /= 2
 *
 * To avoid iteration over all event types, we instead shift numerator of event
 * j lazily when someone asks for a proportion of event j or when event j
 * occurs. This can bit trivially implemented by remembering last period in
 * which something happened with proportion of type j.
 */
#include <linux/flex_proportions.h>
//初始化fprop_global，p->period=0；p->events->count=1
int fprop_global_init(struct fprop_global *p, gfp_t gfp)
{
	int err;

	p->period = 0;
	/* Use 1 to avoid dealing with periods with 0 events... */
	err = percpu_counter_init(&p->events, 1, gfp);
	if (err)
		return err;
	seqcount_init(&p->sequence);
	return 0;
}

void fprop_global_destroy(struct fprop_global *p)
{
	percpu_counter_destroy(&p->events);
}

/*
 * Declare @periods new periods. It is upto the caller to make sure period
 * transitions cannot happen in parallel.
 *
 * The function returns true if the proportions are still defined and false
 * if aging zeroed out all events. This can be used to detect whether declaring
 * further periods has any effect.
 */
 //flex proportions的中心思想是:
 //有两种事件，一个是event事件，还有一个是period事件
 //当有event事件时，当前事件的count和总事件的count均增加
 //当有event事件时，所有事件的count均要变成原来的1/(1<<periods)
 
 //p的period要增加periods p->period += periods;
 //同时其events要右移 p->events=events >> periods
bool fprop_new_period(struct fprop_global *p, int periods)
{
	s64 events;
	unsigned long flags;

	local_irq_save(flags);
	events = percpu_counter_sum(&p->events);
	/*
	 * Don't do anything if there are no events.
	 */
	if (events <= 1) {
		local_irq_restore(flags);
		return false;
	}
	write_seqcount_begin(&p->sequence);
	if (periods < 64)
		events -= events >> periods;
	/* Use addition to avoid losing events happening between sum and set */
	percpu_counter_add(&p->events, -events);
	p->period += periods;
	write_seqcount_end(&p->sequence);
	local_irq_restore(flags);

	return true;
}

/*
 * ---- SINGLE ----
 */
//初始化pl
int fprop_local_init_single(struct fprop_local_single *pl)
{
	pl->events = 0;
	pl->period = 0;
	raw_spin_lock_init(&pl->lock);
	return 0;
}

void fprop_local_destroy_single(struct fprop_local_single *pl)
{
}
//在更新period时，只更新fprop_global的，并没有更新fprop_local_single的period
//这个函数同步这两者的period，同时也会更新fprop_local_single的count
//这个一般发生在fprop_local_single有event时
static void fprop_reflect_period_single(struct fprop_global *p,
					struct fprop_local_single *pl)
{
	unsigned int period = p->period;
	unsigned long flags;

	/* Fast path - period didn't change */
	if (pl->period == period)
		return;
	raw_spin_lock_irqsave(&pl->lock, flags);
	/* Someone updated pl->period while we were spinning? */
	if (pl->period >= period) {
		raw_spin_unlock_irqrestore(&pl->lock, flags);
		return;
	}
	/* Aging zeroed our fraction? */
	if (period - pl->period < BITS_PER_LONG)
		pl->events >>= period - pl->period;
	else
		pl->events = 0;
	pl->period = period;
	raw_spin_unlock_irqrestore(&pl->lock, flags);
}

/* Event of type pl happened */
//pl事件发生了，p的event++，p的event也++，
//会先更新pl的period
void __fprop_inc_single(struct fprop_global *p, struct fprop_local_single *pl)
{
	fprop_reflect_period_single(p, pl);
	pl->events++;
	percpu_counter_add(&p->events, 1);
}

/* Return fraction of events of type pl */
//返回pl中的event占总event的比例
//numerator为pl的event
//denominator为p的event

void fprop_fraction_single(struct fprop_global *p,
			   struct fprop_local_single *pl,
			   unsigned long *numerator, unsigned long *denominator)
{
	unsigned int seq;
	s64 num, den;

	do {
		seq = read_seqcount_begin(&p->sequence);
		fprop_reflect_period_single(p, pl);
		num = pl->events;
//注:这里调用的是percpu_counter_read_positive，所以拿到的值不是个精确值        
		den = percpu_counter_read_positive(&p->events);
	} while (read_seqcount_retry(&p->sequence, seq));

	/*
	 * Make fraction <= 1 and denominator > 0 even in presence of percpu
	 * counter errors
	 */
	if (den <= num) {
		if (num)
			den = num;
		else
			den = 1;
	}
	*denominator = den;
	*numerator = num;
}

/*
 * ---- PERCPU ----
 */
#define PROP_BATCH (8*(1+ilog2(nr_cpu_ids)))

//初始化pl的period和event都为0
int fprop_local_init_percpu(struct fprop_local_percpu *pl, gfp_t gfp)
{
	int err;

	err = percpu_counter_init(&pl->events, 0, gfp);
	if (err)
		return err;
	pl->period = 0;
	raw_spin_lock_init(&pl->lock);
	return 0;
}

void fprop_local_destroy_percpu(struct fprop_local_percpu *pl)
{
	percpu_counter_destroy(&pl->events);
}
//更新pl的period和events
static void fprop_reflect_period_percpu(struct fprop_global *p,
					struct fprop_local_percpu *pl)
{
	unsigned int period = p->period;
	unsigned long flags;

	/* Fast path - period didn't change */
	if (pl->period == period)
		return;
	raw_spin_lock_irqsave(&pl->lock, flags);
	/* Someone updated pl->period while we were spinning? */
	if (pl->period >= period) {
		raw_spin_unlock_irqrestore(&pl->lock, flags);
		return;
	}
	/* Aging zeroed our fraction? */
	if (period - pl->period < BITS_PER_LONG) {
		s64 val = percpu_counter_read(&pl->events);

		if (val < (nr_cpu_ids * PROP_BATCH))
			val = percpu_counter_sum(&pl->events);

		__percpu_counter_add(&pl->events,
			-val + (val >> (period-pl->period)), PROP_BATCH);
	} else
		percpu_counter_set(&pl->events, 0);
	pl->period = period;
	raw_spin_unlock_irqrestore(&pl->lock, flags);
}

/* Event of type pl happened */
void __fprop_inc_percpu(struct fprop_global *p, struct fprop_local_percpu *pl)
{
	fprop_reflect_period_percpu(p, pl);
	__percpu_counter_add(&pl->events, 1, PROP_BATCH);
	percpu_counter_add(&p->events, 1);
}

void fprop_fraction_percpu(struct fprop_global *p,
			   struct fprop_local_percpu *pl,
			   unsigned long *numerator, unsigned long *denominator)
{
	unsigned int seq;
	s64 num, den;

	do {
		seq = read_seqcount_begin(&p->sequence);
		fprop_reflect_period_percpu(p, pl);
//注:这里调用的是percpu_counter_read_positive，所以拿到的值不是个精确值       
		num = percpu_counter_read_positive(&pl->events);
		den = percpu_counter_read_positive(&p->events);
	} while (read_seqcount_retry(&p->sequence, seq));

	/*
	 * Make fraction <= 1 and denominator > 0 even in presence of percpu
	 * counter errors
	 */
	if (den <= num) {
		if (num)
			den = num;
		else
			den = 1;
	}
	*denominator = den;
	*numerator = num;
}

/*
 * Like __fprop_inc_percpu() except that event is counted only if the given
 * type has fraction smaller than @max_frac/FPROP_FRAC_BASE
 */
void __fprop_inc_percpu_max(struct fprop_global *p,
			    struct fprop_local_percpu *pl, int max_frac)
{
	if (unlikely(max_frac < FPROP_FRAC_BASE)) {
		unsigned long numerator, denominator;

		fprop_fraction_percpu(p, pl, &numerator, &denominator);
		if (numerator >
		    (((u64)denominator) * max_frac) >> FPROP_FRAC_SHIFT)
			return;
	} else
		fprop_reflect_period_percpu(p, pl);
	__percpu_counter_add(&pl->events, 1, PROP_BATCH);
	percpu_counter_add(&p->events, 1);
}
