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

	int32_t pos = rte_hash_lookup(table->hash, key);
	if (pos < 0) {
		pos = rte_hash_add_key(table->hash, key);
		if (pos < 0) {
			return -ENOENT;
		}
		if (is_new) {
			*is_new = true;
		}
		table->entries[pos].last_seen_tsc = now_tsc;
		table->entries[pos].packets = 1;
		table->entries[pos].bytes = pkt_len;
		return 0;
	}

	if (is_new) {
		*is_new = false;
	}
	table->entries[pos].last_seen_tsc = now_tsc;
	table->entries[pos].packets += 1;
	table->entries[pos].bytes += pkt_len;
	return 0;
}
