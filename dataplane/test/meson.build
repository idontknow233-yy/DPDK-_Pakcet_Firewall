project('dpdk_packet_firewall_tests', 'c',
    default_options: ['c_std=gnu11', 'warning_level=2'])

dpdk_dep = dependency('libdpdk', required: false)

# ACL Tests
test_acl_sources = [
  'test_acl.c',
  '../acl/acl.c',
  'mock_dpdk.c',
]

test_acl_exec = executable(
  'test_acl',
  test_acl_sources,
  dependencies: dpdk_dep,
)
test('acl_basic', test_acl_exec, suite: 'dataplane')

# Session Tests
test_session_sources = [
  'test_session.c',
  '../session/session.c',
  'mock_dpdk.c',
]

test_session_exec = executable(
  'test_session',
  test_session_sources,
  dependencies: dpdk_dep,
)
test('session_basic', test_session_exec, suite: 'dataplane')

# Route Tests
test_route_sources = [
  'test_route.c',
  '../route/route.c',
  'mock_dpdk.c',
]

test_route_exec = executable(
  'test_route',
  test_route_sources,
  dependencies: dpdk_dep,
)
test('route_basic', test_route_exec, suite: 'dataplane')
