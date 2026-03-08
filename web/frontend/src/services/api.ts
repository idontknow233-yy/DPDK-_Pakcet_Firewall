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

export async function deleteAclRule(index: number): Promise<void> {
  await api.delete(`/api/acl/${index}`)
}

export async function clearAcl(): Promise<void> {
  await api.delete('/api/acl/')
}
