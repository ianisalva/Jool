#include "mod/common/stats.h"

#include <linux/kref.h>
#include <net/snmp.h>
#include "mod/common/wkmalloc.h"

struct jool_mib {
	unsigned long mibs[__JSTAT_MAX];
};

struct jool_stats {
	DEFINE_SNMP_STAT(struct jool_mib, mib);
	struct kref refcounter;
};

struct jool_stats *jstat_alloc(void)
{
	struct jool_stats *result;

	result = wkmalloc(struct jool_stats, GFP_KERNEL);
	if (!result)
		return NULL;

	result->mib = alloc_percpu(struct jool_mib);
	if (!result->mib) {
		wkfree(struct jool_stats, result);
		return NULL;
	}
	kref_init(&result->refcounter);

	return result;
}

void jstat_get(struct jool_stats *stats)
{
	kref_get(&stats->refcounter);
}

static void jstat_release(struct kref *refcount)
{
	struct jool_stats *stats;
	stats = container_of(refcount, struct jool_stats, refcounter);

	free_percpu(stats->mib);
	wkfree(struct jool_stats, stats);
}

void jstat_put(struct jool_stats *stats)
{
	kref_put(&stats->refcounter, jstat_release);
}

void jstat_inc(struct jool_stats *stats, enum jool_stat_id stat)
{
	SNMP_INC_STATS(stats->mib, stat);
}

void jstat_dec(struct jool_stats *stats, enum jool_stat_id stat)
{
	SNMP_DEC_STATS(stats->mib, stat);
}

void jstat_add(struct jool_stats *stats, enum jool_stat_id stat, int addend)
{
	SNMP_ADD_STATS(stats->mib, stat, addend);
}

#define GET_COUNTER(mib, c, i) (*((unsigned long *)(per_cpu_ptr(mib, c)) + (i)))

/**
 * Returns the list of stats as an array. You will have to free it.
 * The array length will be __JSTAT_MAX.
 */
__u64 *jstat_query(struct jool_stats *stats)
{
	__u64 *result;
	int c, i;

	result = kcalloc(__JSTAT_MAX, sizeof(__u64), GFP_KERNEL);
	if (!result)
		return NULL;

	for_each_possible_cpu(c) {
		for (i = 0; i < __JSTAT_MAX; i++)
			result[i] += GET_COUNTER(stats->mib, c, i);
	}

	return result;
}
