import axios from 'axios'

export type AclRow = {
  index: number
  action: string
  src: string
  dst: string
  proto: string
  sport: string
  dport: string
}

export type AclSnapshot = {
  version: number
  count: number
  rules: AclRow[]
}

const api = axios.create({
  baseURL: import.meta.env.VITE_API_BASE || ''
})

export async function getAclSnapshot(): Promise<AclSnapshot> {
  const { data } = await api.get('/api/acl')
  return data
}

export async function getAcl6Snapshot(): Promise<AclSnapshot> {
  const { data } = await api.get('/api/acl6')
  return data
}

export type AclHitsSnapshot = {
  version: number
  rule_version: number
  count: number
  pkts: number[]
  bytes: number[]
}

export async function getAclHits(): Promise<AclHitsSnapshot> {
  const { data } = await api.get('/api/acl/hits')
  return data
}

export async function getAcl6Hits(): Promise<AclHitsSnapshot> {
  const { data } = await api.get('/api/acl6/hits')
  return data
}

export async function getIfcfg6(): Promise<{ version: number; count: number; ifaces: { port: number; cidr: string }[] }> {
  const { data } = await api.get('/api/ifcfg6')
  return data
}

export async function setIfcfg6(port: number, cidr: string): Promise<void> {
  await api.post('/api/ifcfg6', { port, cidr })
}

export async function clearIfcfg6(port: number): Promise<void> {
  await api.delete(`/api/ifcfg6/${port}`)
}

export async function getIfcfg4(): Promise<{ version: number; count: number; ifaces: { port: number; cidr: string }[] }> {
  const { data } = await api.get('/api/ifcfg4')
  return data
}

export async function setIfcfg4(port: number, cidr: string): Promise<void> {
  await api.post('/api/ifcfg4', { port, cidr })
}

export async function clearIfcfg4(port: number): Promise<void> {
  await api.delete(`/api/ifcfg4/${port}`)
}

export async function getRoute6(): Promise<{ version: number; count: number; routes: { index: number; dst: string; nh: string; port: number }[] }> {
  const { data } = await api.get('/api/route6')
  return data
}

export async function addRoute6(dst: string, nh: string, port: number): Promise<void> {
  await api.post('/api/route6', { dst, nh, port })
}

export async function deleteRoute6(index: number): Promise<void> {
  await api.delete(`/api/route6/${index}`)
}

export async function clearRoute6(): Promise<void> {
  await api.delete('/api/route6/')
}

export async function getRoute4(): Promise<{ version: number; count: number; routes: { index: number; dst: string; nh: string; port: number }[] }> {
  const { data } = await api.get('/api/route4')
  return data
}

export async function addRoute4(dst: string, nh: string, port: number): Promise<void> {
  await api.post('/api/route4', { dst, nh, port })
}

export async function deleteRoute4(index: number): Promise<void> {
  await api.delete(`/api/route4/${index}`)
}

export async function clearRoute4(): Promise<void> {
  await api.delete('/api/route4/')
}

export type AttackSnapshot = {
  version: number
  mitigation: number
  scan_ports_sec: number
  ban_sec: number
  syn_pps: number
  udp_pps: number
  scan_events: number
  scan_banned: number
  top4: string
  top4_ports: number
  top6: string
  top6_ports: number
}

export async function getAttack(): Promise<AttackSnapshot> {
  const { data } = await api.get('/api/attack')
  return data
}

export async function setAttack(payload: { mitigation: number; scan_ports_sec: number; ban_sec: number }): Promise<void> {
  await api.post('/api/attack', payload)
}

export async function getHealth(): Promise<boolean> {
  try {
    await api.get('/api/health')
    return true
  } catch {
    return false
  }
}

export type SessionRow = {
  index: number
  proto: number
  src: string
  dst: string
  packets: number
  bytes: number
  last_seen_ms: number
}

export type SessionSnapshot = {
  version: number
  count: number
  sessions: SessionRow[]
}

export async function getSessions(limit = 200): Promise<SessionSnapshot> {
  const { data } = await api.get('/api/sessions', { params: { limit } })
  return data
}

export async function getSessions6(limit = 200): Promise<SessionSnapshot> {
  const { data } = await api.get('/api/sessions6', { params: { limit } })
  return data
}

export type PortStatsRow = {
  port: number
  rx: number
  tx: number
  dropped: number
  link?: string
  speed?: number
  duplex?: string
  mac?: string
}

export type PortStatsSnapshot = {
  version: number
  mask: number
  ports: PortStatsRow[]
}

export async function getPortStats(): Promise<PortStatsSnapshot> {
  const { data } = await api.get('/api/ports')
  return data
}

export type DenyRow = {
  index: number
  age_ms: number
  in_port: number
  proto: number
  src: string
  dst: string
  rule: number
}

export type DenySnapshot = {
  version: number
  count: number
  denies: DenyRow[]
}

export async function getDenies(limit = 50): Promise<DenySnapshot> {
  const { data } = await api.get('/api/denies', { params: { limit } })
  return data
}

export async function getDenies6(limit = 50): Promise<DenySnapshot> {
  const { data } = await api.get('/api/denies6', { params: { limit } })
  return data
}

export type DdosConfig = {
  version: number
  syn_pps: number
  syn_burst: number
  udp_pps: number
  udp_burst: number
}

export async function getDdos(): Promise<DdosConfig> {
  const { data } = await api.get('/api/ddos')
  return data
}

export async function setDdos(payload: {
  syn_pps: number
  syn_burst: number
  udp_pps: number
  udp_burst: number
}): Promise<void> {
  await api.post('/api/ddos', payload)
}

export async function addAclRule(payload: {
  action: string
  src: string
  dst: string
  proto: number
  src_port_min: number
  src_port_max: number
  dst_port_min: number
  dst_port_max: number
}): Promise<void> {
  await api.post('/api/acl', payload)
}

export async function addAcl6Rule(payload: {
  action: string
  src: string
  dst: string
  proto: number
  src_port_min: number
  src_port_max: number
  dst_port_min: number
  dst_port_max: number
}): Promise<void> {
  await api.post('/api/acl6', payload)
}

export async function deleteAclRule(index: number): Promise<void> {
  await api.delete(`/api/acl/${index}`)
}

export async function deleteAcl6Rule(index: number): Promise<void> {
  await api.delete(`/api/acl6/${index}`)
}

export async function clearAcl(): Promise<void> {
  await api.delete('/api/acl/')
}

export async function clearAcl6(): Promise<void> {
  await api.delete('/api/acl6/')
}
