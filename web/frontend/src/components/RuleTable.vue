<template>
  <el-table
    :data="rows"
    :row-class-name="rowClass"
    v-loading="loading"
    stripe
    style="width:100%"
  >
    <el-table-column prop="index" label="#" width="72" sortable />
    <el-table-column label="动作" width="90">
      <template #default="{row}">
        <el-tag :type="row.action === 'deny' ? 'danger' : 'success'" effect="light">
          {{ row.action }}
        </el-tag>
      </template>
    </el-table-column>
    <el-table-column prop="src" label="源" min-width="180" />
    <el-table-column prop="dst" label="目的" min-width="180" />
    <el-table-column label="协议" width="80">
      <template #default="{row}">
        <span>{{ formatProto(row.proto) }}</span>
      </template>
    </el-table-column>
    <el-table-column prop="sport" label="源端口" width="120" />
    <el-table-column prop="dport" label="目的端口" width="120" />
    <el-table-column label="操作" width="120">
      <template #default="{row}">
        <el-button size="small" type="danger" @click="$emit('delete', row.index)">删除</el-button>
      </template>
    </el-table-column>
  </el-table>
</template>

<script lang="ts" setup>
defineProps<{rows: Array<any>; loading?: boolean}>()
defineEmits<{(e: 'delete', index: number): void}>()
function formatProto(p: string) {
  if (p === 'any') return 'any'
  if (p === '6' || p === 6 as any) return 'TCP'
  if (p === '17' || p === 17 as any) return 'UDP'
  return p
}
function rowClass({ row }: any) {
  if (row.action === 'deny') return 'row-deny'
  return ''
}
</script>

<style scoped>
.row-deny td { background-color: #fef0f0 !important; }
</style>
