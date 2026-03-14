<template>
  <el-form ref="formRef" :model="form" :rules="rules" label-width="92px" @submit.prevent>
    <el-form-item label="动作" prop="allow">
      <el-radio-group v-model="form.allow">
        <el-radio-button :label="1">allow</el-radio-button>
        <el-radio-button :label="0">deny</el-radio-button>
      </el-radio-group>
    </el-form-item>
    <el-form-item label="源IP" prop="src">
      <div class="ip-input">
        <el-select v-model="form.srcAny" style="width: 100px" @change="handleSrcAnyChange">
          <el-option :value="false" label="指定IP" />
          <el-option :value="true" label="any" />
        </el-select>
        <el-input v-model="form.src" :placeholder="srcPlaceholder" clearable :disabled="form.srcAny" />
      </div>
    </el-form-item>
    <el-form-item label="目的IP" prop="dst">
      <div class="ip-input">
        <el-select v-model="form.dstAny" style="width: 100px" @change="handleDstAnyChange">
          <el-option :value="false" label="指定IP" />
          <el-option :value="true" label="any" />
        </el-select>
        <el-input v-model="form.dst" :placeholder="dstPlaceholder" clearable :disabled="form.dstAny" />
      </div>
    </el-form-item>
    <el-form-item label="协议" prop="proto">
      <el-select v-model="form.proto" style="width: 100%">
        <el-option :value="0" label="any" />
        <el-option :value="6" label="TCP" />
        <el-option :value="17" label="UDP" />
      </el-select>
    </el-form-item>
    <el-form-item label="源端口" prop="src_port_min">
      <div class="ports">
        <el-input-number v-model="form.src_port_min" :min="0" :max="65535" :disabled="portsDisabled" controls-position="right" />
        <span class="ports__sep">-</span>
        <el-input-number v-model="form.src_port_max" :min="0" :max="65535" :disabled="portsDisabled" controls-position="right" />
      </div>
    </el-form-item>
    <el-form-item label="目的端口" prop="dst_port_min">
      <div class="ports">
        <el-input-number v-model="form.dst_port_min" :min="0" :max="65535" :disabled="portsDisabled" controls-position="right" />
        <span class="ports__sep">-</span>
        <el-input-number v-model="form.dst_port_max" :min="0" :max="65535" :disabled="portsDisabled" controls-position="right" />
      </div>
    </el-form-item>
    <el-form-item>
      <el-button type="primary" :loading="submitting" @click="submit">新增规则</el-button>
      <el-button :disabled="submitting" @click="reset">重置</el-button>
    </el-form-item>
  </el-form>
</template>

<script lang="ts" setup>
import { computed, reactive, ref, watch } from 'vue'
import type { FormInstance, FormRules } from 'element-plus'

const props = withDefaults(defineProps<{ submitting?: boolean; ipVersion?: 4 | 6 }>(), { ipVersion: 4 })
const emit = defineEmits<{(e: 'submit', payload: any): void}>()

const formRef = ref<FormInstance>()
const form = reactive({
  allow: 1,
  src: '',
  dst: '',
  srcAny: false,
  dstAny: false,
  proto: 0,
  src_port_min: 0,
  src_port_max: 0,
  dst_port_min: 0,
  dst_port_max: 0
})

const cidr4Re = /^(\d{1,3}\.){3}\d{1,3}\/([0-9]|[12]\d|3[0-2])$/
const cidr6Re = /^[0-9a-fA-F:]+\/([0-9]|[1-9]\d|1[01]\d|12[0-8])$/
function validateCIDR(_: any, value: string, callback: any) {
  const v = (value || '').trim()
  if (!v) return callback(new Error('必填'))
  if (props.ipVersion === 4) {
    if (!cidr4Re.test(v)) return callback(new Error('格式应为 x.x.x.x/0-32'))
    const [ip] = v.split('/')
    const parts = ip.split('.').map((x) => Number(x))
    if (parts.some((n) => Number.isNaN(n) || n < 0 || n > 255)) return callback(new Error('IP 段应为 0-255'))
  } else {
    if (!cidr6Re.test(v)) return callback(new Error('格式应为 ipv6/0-128'))
  }
  callback()
}

function validateIPBothAny(_: any, __: any, callback: any) {
  if (form.srcAny && form.dstAny) {
    callback(new Error('源IP和目的IP不能同时为any'))
  } else {
    callback()
  }
}

function handleSrcAnyChange(val: boolean) {
  if (val) {
    form.src = ''
    formRef.value?.clearValidate(['src'])
  }
}

function handleDstAnyChange(val: boolean) {
  if (val) {
    form.dst = ''
    formRef.value?.clearValidate(['dst'])
  }
}

const anyCidr = computed(() => (props.ipVersion === 6 ? '::/0' : '0.0.0.0/0'))
const srcPlaceholder = computed(() => (props.ipVersion === 6 ? '例如 2001:db8::/64' : '例如 192.168.1.0/24'))
const dstPlaceholder = computed(() => (props.ipVersion === 6 ? '例如 2001:db8:1::/64' : '例如 10.0.0.1/32'))

const portsDisabled = computed(() => form.proto === 0)
watch(() => form.proto, (p) => {
  if (p === 0) {
    form.src_port_min = 0
    form.src_port_max = 0
    form.dst_port_min = 0
    form.dst_port_max = 0
  } else {
    if (form.src_port_max === 0) form.src_port_max = 65535
    if (form.dst_port_max === 0) form.dst_port_max = 65535
  }
})

const rules: FormRules = {
  src: [{
    validator: (_: any, __: any, cb: any) => {
      if (form.srcAny) return cb()
      return validateCIDR(_, form.src, cb)
    }, trigger: 'blur'
  }],
  dst: [{
    validator: (_: any, __: any, cb: any) => {
      if (form.dstAny) return cb()
      return validateCIDR(_, form.dst, cb)
    }, trigger: 'blur'
  }, { validator: validateIPBothAny, trigger: 'blur' }],
  src_port_min: [{
    validator: (_: any, __: any, cb: any) => {
      if (portsDisabled.value) return cb()
      if (form.src_port_min > form.src_port_max) return cb(new Error('最小端口不能大于最大端口'))
      cb()
    }, trigger: 'change'
  }],
  dst_port_min: [{
    validator: (_: any, __: any, cb: any) => {
      if (portsDisabled.value) return cb()
      if (form.dst_port_min > form.dst_port_max) return cb(new Error('最小端口不能大于最大端口'))
      cb()
    }, trigger: 'change'
  }]
}

function reset() {
  form.allow = 1
  form.src = ''
  form.dst = ''
  form.srcAny = false
  form.dstAny = false
  form.proto = 0
  form.src_port_min = 0
  form.src_port_max = 0
  form.dst_port_min = 0
  form.dst_port_max = 0
  formRef.value?.clearValidate()
}

async function submit() {
  const ok = await formRef.value?.validate().catch(() => false)
  if (!ok) return
  const payload = {
    ...form,
    src: form.srcAny ? anyCidr.value : form.src,
    dst: form.dstAny ? anyCidr.value : form.dst
  }
  emit('submit', payload)
}
</script>

<style scoped>
.ports { display:flex; align-items:center; gap:8px; width: 100%; }
.ports__sep { color: #909399; }
.ip-input { display: flex; gap: 8px; width: 100%; }
.ip-input .el-input { flex: 1; }
</style>
