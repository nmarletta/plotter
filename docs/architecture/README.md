# Pen Plotter Controller Architecture - Document Index

This architecture document has been sharded into focused sections for easier navigation during development.

## Document Sections

1. **[Introduction](01-introduction.md)** - Document purpose, project analysis, constraints, and change log
2. **Component Architecture** - Detailed component designs (SerialManager, GCodeStreamer, ProgressTracker, StateSettings, StateControl) with interaction diagram
3. **Data Models** - C++ classes and file formats representing runtime state and persistent data
4. **Tech Stack** - Complete technology stack alignment and rationale
5. **Source Tree Integration** - File organization, naming conventions, and integration guidelines
6. **Coding Standards** - Memory management, serial communication patterns, error handling, state management rules with code examples
7. **Testing Strategy** - Unit tests, integration tests, regression testing, test data and fixtures
8. **Security Integration** - Input validation, buffer safety, security best practices for embedded systems

## Master Document

The complete, unsharded architecture is available at: `docs/architecture.md`

## Related Documentation

- **Project Brief**: `docs/brief.md` - Strategic context
- **PRD**: `docs/prd.md` - Requirements specification
- **PRD Shards**: `docs/prd/` - Focused requirement sections
- **User Stories**: `docs/stories/` - Implementation tasks (created by SM)

## Critical Integration Notes

- Serial1 must be initialized at 115200 baud before GRBL communication
- All Serial.println() debug output must be removed (Serial1 is for GRBL only)
- State functions use activation flags pattern (see existing code)
- Memory constraint: 32KB SRAM - profile regularly
- Pin definitions in main.cpp must be corrected before any testing
- GCodeStreamer and StatePlot are untested - validation required

## Next Steps for Development

1. **Story Manager**: Create user stories starting with SerialManager foundation
2. **Developers**: Follow coding standards and patterns documented here
3. **QA**: Reference testing strategy for validation approach

## Quick Reference

- **State Machine Pattern**: See Coding Standards section
- **Serial Communication**: See Data Models (SerialManager) and Coding Standards
- **Component Interactions**: See Component Architecture diagram
- **File Organization**: See Source Tree Integration
