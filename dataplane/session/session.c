#include "session.h"

#include <errno.h>
#include <string.h>

#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

int session_table_init(struct session_table *table, const char *name, uint32_t capacity, int socket_id) {
	if (!table || !name || capacity == 0) {
		return -EINVAL;
	}

	struct rte_hash_parameters params = {
		.name = name,
		.entries = capacity,
		.key_len = sizeof(struct session_key),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = socket_id,
	};

	table->hash = rte_hash_create(&params);
	if (!table->hash) {
		return -ENOMEM;
	}

	table->entries = rte_zmalloc_socket("session_entries",
		sizeof(struct session_entry) * capacity, 0, socket_id);
	if (!table->entries) {
		rte_hash_free(table->hash);
		table->hash = NULL;
		return -ENOMEM;
	}

	table->capacity = capacity;
	rte_spinlock_init(&table->lock);
	return 0;
}

void session_table_free(struct session_table *table) {
	if (!table) {
		return;
	}
	if (table->entries) {
		rte_free(table->entries);
		table->entries = NULL;
	}
	if (table->hash) {
		rte_hash_free(table->hash);
		table->hash = NULL;
	}
	table->capacity = 0;
}

int session_track(struct session_table *table, const struct session_key *key, uint32_t pkt_len, uint64_t now_tsc, bool *is_new) {
	if (!table || !table->hash || !table->entries || !key) {
		return -EINVAL;
	}

	rte_spinlock_lock(&table->lock);
	int32_t pos = rte_hash_lookup(table->hash, key);
	if (pos < 0) {
		pos = rte_hash_add_key(table->hash, key);
		if (pos < 0) {
			rte_spinlock_unlock(&table->lock);
			return -ENOENT;
		}
		if (is_new) {
			*is_new = true;
		}
		table->entries[pos].last_seen_tsc = now_tsc;
		table->entries[pos].packets = 1;
		table->entries[pos].bytes = pkt_len;
		rte_spinlock_unlock(&table->lock);
		return 0;
	}

	if (is_new) {
		*is_new = false;
	}
	table->entries[pos].last_seen_tsc = now_tsc;
	table->entries[pos].packets += 1;
	table->entries[pos].bytes += pkt_len;
	rte_spinlock_unlock(&table->lock);
	return 0;
}

uint32_t session_soft_expire(struct session_table *table, uint64_t now_tsc, uint64_t timeout_tsc) {
	if (!table || !table->hash || !table->entries) {
		return 0;
	}
	if (timeout_tsc == 0) {
		return 0;
	}
	uint32_t expired = 0;
	uint32_t next = 0;
	const void *key = NULL;
	void *data = NULL;
	rte_spinlock_lock(&table->lock);
	for (;;) {
		int32_t pos = rte_hash_iterate(table->hash, &key, &data, &next);
		if (pos == -ENOENT) {
			break;
		}
		if (pos < 0) {
			break;
		}
		struct session_entry *e = &table->entries[pos];
		if (e->last_seen_tsc == 0) {
			continue;
		}
		if (now_tsc - e->last_seen_tsc > timeout_tsc) {
			rte_hash_del_key(table->hash, key);
			e->last_seen_tsc = 0;
			e->packets = 0;
			e->bytes = 0;
			expired++;
		}
	}
	rte_spinlock_unlock(&table->lock);
	return expired;
}

uint32_t session_export(struct session_table *table, struct session_key *keys, struct session_entry *entries, uint32_t max_entries,
	uint64_t now_tsc, uint64_t timeout_tsc) {
	if (!table || !table->hash || !table->entries || !keys || !entries || max_entries == 0) {
		return 0;
	}
	uint32_t next = 0;
	const void *key = NULL;
	void *data = NULL;
	uint32_t out = 0;
	rte_spinlock_lock(&table->lock);
	for (;;) {
		int32_t pos = rte_hash_iterate(table->hash, &key, &data, &next);
		if (pos == -ENOENT) {
			break;
		}
		if (pos < 0) {
			break;
		}
		const struct session_entry *e = &table->entries[pos];
		if (e->last_seen_tsc == 0) {
			continue;
		}
		if (timeout_tsc && (now_tsc - e->last_seen_tsc > timeout_tsc)) {
			continue;
		}
		if (out >= max_entries) {
			break;
		}
		keys[out] = *(const struct session_key *)key;
		entries[out] = *e;
		out++;
	}
	rte_spinlock_unlock(&table->lock);
	return out;
}
