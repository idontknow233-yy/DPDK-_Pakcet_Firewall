fs = import('fs')

if fs.exists('main.c')
  executable(
    'dpdk_packet_firewall',
    'main.c',
    'acl/acl.c',
    'session/session.c',
    dependencies: dpdk_dep,
    install: false
  )
endif
