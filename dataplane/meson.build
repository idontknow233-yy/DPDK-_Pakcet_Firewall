fs = import('fs')

if fs.exists('main.c')
  executable(
    'dpdk_packet_firewall',
    'main.c',
    'acl/acl.c',
    'arp/arp.c',
    'route/route.c',
    'session/session.c',
    dependencies: dpdk_dep,
    install: false
  )
endif
