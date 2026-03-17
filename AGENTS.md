# AGENTS.md - Development Guide for DPDK Packet Firewall

## Project Overview
This is a DPDK-based high-performance packet firewall project with C/C++ data/control plane and Vue.js web frontend. The project uses Meson + Ninja for C/C++ builds and Vite for frontend development.

## Build Commands

### C/C++ Core (Meson + Ninja)
```bash
# Build the entire project
meson setup build
ninja -C build

# Clean build
rm -rf build
meson setup build
ninja -C build

# Build specific targets
ninja -C build dpdk_packet_firewall  # Data plane
ninja -C build control_plane       # Control plane
```

### Frontend (Vue.js + TypeScript)
```bash
# Install dependencies
cd web/frontend
npm install

# Development server
npm run dev

# Build for production
npm run build

# Type checking
npm run typecheck
```

## Test Commands

### C/C++ Unit Tests
```bash
# Run all tests
ninja -C build test

# Run specific test module
ninja -C build test_dataplane
ninja -C build test_controlplane

# Run tests with verbose output
ninja -C build test -v
```

### Frontend Tests
```bash
# Run frontend tests
cd web/frontend
npm test

# Run specific test file
npm test -- --testPathPattern=MyComponent.test.ts
```

## Linting Commands

### C/C++ (Clang Format)
```bash
# Format all C/C++ code
find . -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting without applying
find . -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" | xargs clang-format -n
```

### TypeScript/Vue.js
```bash
# Lint frontend code
cd web/frontend
npm run lint

# Fix lint issues automatically
npm run lint -- --fix
```

## Code Style Guidelines

### General Principles
- Follow Linux kernel coding style for C/C++ code
- Use meaningful, descriptive names for variables, functions, and modules
- Write comprehensive comments for complex logic and public APIs
- Maintain consistent indentation and formatting
- Prefer clarity over cleverness

### C/C++ Coding Style

#### Naming Conventions
- **Variables**: snake_case (e.g., `packet_buffer`, `connection_table`)
- **Functions**: snake_case (e.g., `parse_ipv4_packet`, `handle_tcp_connection`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_CONNECTIONS`, `DEFAULT_TIMEOUT`)
- **Structs/Enums**: PascalCase (e.g., `IpAddress`, `PacketType`)
- **Macros**: UPPER_SNAKE_CASE (e.g., `CHECK_NULL_PTR`, `ASSERT_NONZERO`)

#### Formatting
- Use 4 spaces for indentation (no tabs)
- Limit lines to 80 characters
- Use braces even for single-line blocks
- Properly space operators and keywords
- Consistent placement of braces

#### Error Handling
```c
// Good: Specific error codes with descriptive messages
int result = some_function();
if (result != 0) {
    RTE_LOG(ERR, MAIN, "Failed to initialize: %s\n", strerror(-result));
    return -result;
}

// Good: Use RTE_ASSERT for critical assertions
RTE_ASSERT(packet != NULL);
RTE_ASSERT(packet->length > 0);
```

#### Memory Management
- Use DPDK memory pools (rte_mempool) for packet buffers
- Always check return values for memory allocation
- Free memory in reverse order of allocation
- Use rte_free() instead of free() for DPDK-allocated memory

### TypeScript/Vue.js Coding Style

#### Naming Conventions
- **Variables/Functions**: camelCase (e.g., `packetBuffer`, `parseIpv4Packet`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_CONNECTIONS`)
- **Components**: PascalCase (e.g., `PacketAnalyzer`, `ConnectionTracker`)
- **Props/Events**: camelCase (e.g., `packetData`, `connectionUpdate`)

#### Formatting
- Use 2 spaces for indentation
- Limit lines to 100 characters
- Use single quotes for strings
- Consistent use of semicolons
- Proper spacing around operators

#### TypeScript Specific
- Enable strict mode in tsconfig.json
- Use interfaces over types when possible
- Properly type function parameters and return values
- Use `readonly` for immutable properties
- Avoid `any` type; use `unknown` or specific types instead

### Documentation

#### C/C++ Comments
```c
/**
 * @brief Parse IPv4 packet headers and validate packet structure
 * @param packet Pointer to the received packet buffer
 * @param packet_len Length of the packet in bytes
 * @return 0 on success, negative error code on failure
 * 
 * This function parses the IPv4 header, validates checksum, and extracts
 * essential packet information for further processing.
 */
int parse_ipv4_packet(struct rte_mbuf *packet, uint16_t packet_len);
```

#### TypeScript/Vue.js Comments
```typescript
/**
 * Parses IPv4 packet headers and validates packet structure
 * @param packet - Received packet buffer
 * @param packetLen - Length of the packet in bytes
 * @returns 0 on success, error code on failure
 */
function parseIpv4Packet(packet: PacketBuffer, packetLen: number): number;
```

## Git Workflow

### Branch Naming
- `main`: Production/stable branch
- `develop`: Integration branch for completed features
- `feature/module-name`: New feature development
- `release/vX.Y.Z`: Release candidate branch

### Commit Message Format
```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

#### Type Options:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation update
- `perf`: Performance improvement
- `refactor`: Code restructuring
- `chore`: Maintenance task

#### Scope Options:
- `dataplane`: Data plane core
- `controlplane`: Control plane
- `protocol`: Network protocols
- `ipc`: Inter-process communication
- `web`: Web frontend/backend
- `utils`: Utility functions
- `docs`: Documentation
- `resource`: Project resources
- `project`: Project configuration

#### Example:
```
feat(dataplane): Implement IPv6 protocol parsing

Add support for IPv6 header parsing and basic validation.
Fixes issue with IPv6 packet processing in the firewall.
```

## Development Best Practices

### C/C++ Development
- Use DPDK APIs consistently (rte_mbuf, rte_ring, rte_hash, etc.)
- Implement proper error handling for all DPDK functions
- Use rte_log for logging with appropriate severity levels
- Follow DPDK memory management patterns
- Test with different packet sizes and network conditions

### Frontend Development
- Use Vue 3 Composition API
- Implement proper TypeScript typing
- Use Pinia for state management
- Follow Element Plus component guidelines
- Write unit tests for critical components

### Testing
- Write unit tests for all core functionality
- Test edge cases and error conditions
- Use continuous integration for automated testing
- Profile performance for critical paths
- Document test coverage requirements

## Environment Setup

### System Requirements
- Ubuntu 22.04 LTS (64-bit)
- Linux kernel 5.15.0+ with huge pages enabled
- DPDK 24.11 installed and configured
- GCC 11+ or Clang
- Node.js 18+ for frontend development

### Environment Variables
```bash
export RTE_SDK=/path/to/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
export HUGE_PAGES=1024  # Adjust based on system memory
```

## Performance Considerations

- Use DPDK burst APIs for packet processing
- Optimize memory access patterns
- Minimize cache misses
- Use appropriate DPDK memory pools
- Profile and optimize hot paths
- Consider NUMA awareness for multi-socket systems

## Security Guidelines

- Validate all input packets thoroughly
- Implement proper bounds checking
- Use secure memory allocation
- Protect against buffer overflows
- Validate all configuration inputs
- Implement proper error handling to prevent information leakage

## Troubleshooting

### Common Build Issues
- Check DPDK environment variables
- Ensure huge pages are enabled
- Verify DPDK PMD drivers are loaded
- Check Meson configuration
- Ensure all dependencies are installed

### Runtime Issues
- Check DPDK EAL initialization
- Verify port configuration
- Monitor memory usage
- Check for packet drops
- Review logs for error messages

## Contributing Guidelines

- Follow existing code style and patterns
- Write comprehensive tests for new features
- Update documentation for significant changes
- Use feature branches for development
- Submit pull requests with clear descriptions
- Address review feedback promptly

## Project Structure

```
DPDK_Packet_Firewall/
├── dataplane/          # Data plane core (C/C++)
│   ├── nic/           # NIC driver and packet I/O
│   ├── proto/         # Protocol parsing
│   ├── conn/          # Connection tracking
│   ├── acl/           # ACL rule engine
│   ├── route/         # Routing (LPM)
│   └── core/          # Core processing pipeline
├── controlplane/      # Control plane (C/C++)
│   ├── cli/           # CLI interface
│   ├── ipc/           # Inter-process communication
│   ├── config/        # Configuration management
│   └── api/           # Web API
├── web/              # Web interface
│   ├── frontend/      # Vue.js frontend
│   └── backend/      # Web backend
├── docs/             # Documentation
└── project/          # Build configuration
```

## References

- DPDK Programmer's Guide
- Linux Kernel Coding Style
- Vue.js Style Guide
- TypeScript Handbook
- Meson Build System Documentation

---

*This document is maintained for agentic development in the DPDK Packet Firewall project. Keep it updated with any changes to build processes, coding standards, or development workflows.*