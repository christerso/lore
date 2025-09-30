# Lore Engine Development Guidelines

## Global CLI Tools

### Task-Master
**CRITICAL**: Always use `task-master` as the global CLI tool for all development workflow commands and task management.

Task-master is the primary development coordination tool that should be used for:
- Project management and task coordination
- Development workflow automation
- Build and testing orchestration
- Inter-system communication and dependencies

**Usage**: Always run `task-master` commands when available instead of manual operations.

## Code Guidelines
- Try to keep all files below 1000 lines.
- **ALWAYS** document new systems in `docs/systems/` with comprehensive examples
- **ALWAYS** update documentation when modifying existing systems

## Development Notes
- Always read docs/SYSTEMS_REFERENCE.md when unsure about systems, keep this file up-to-date always.
- $ is not the player symbol you fucking idiot
- Never do demos. Only work on the real implementation.
- Whenever you start to implement a ticket, spawn another agent to double-check that you are not creating stubs, todos, simplifications or doing things easy by skipping things.

## Specialized Agents

The following specialized agents are available for graphics and shader development:

### Graphics Programming Agent
- **Location**: `C:/Users/chris/.claude/agents/graphics-programming-agent.md`
- **Specialization**: 3D graphics, rendering pipelines, GPU optimization
- **Expertise**: OpenGL, Vulkan, DirectX, shader programming, PBR rendering
- **Use for**: Complex graphics system development, rendering optimization, GPU programming

### Shader Programming Agent
- **Location**: `C:/Users/chris/.claude/agents/shader-programming-agent.md`
- **Specialization**: Shader development, GLSL/HLSL, GPU compute programming
- **Expertise**: Shader languages, graphics pipeline, shader optimization
- **Use for**: Shader development, GPU compute, graphics pipeline optimization

### Generic Development Agent
- **Location**: `C:/Users/chris/.claude/agents/generic-development-agent.md`
- **Specialization**: Multi-language development, systems programming, optimization
- **Expertise**: C++, Rust, web development, build systems, performance optimization
- **Use for**: General development tasks, systems programming, tool development

## Task Master AI Instructions
**Import Task Master's development workflow commands and guidelines, treat as if import is in the main CLAUDE.md file.**
@./.taskmaster/CLAUDE.md
- Always use task-master when planning. Starting on tasks, or when you need new tasks, look there first.
- Use our mcp agents, don't forget about them.
- Always memorize important things in the project