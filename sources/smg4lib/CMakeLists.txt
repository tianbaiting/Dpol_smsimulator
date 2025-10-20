project(smg4)

# Build data first as other modules depend on it
add_subdirectory(data)
add_subdirectory(action)
add_subdirectory(construction)
add_subdirectory(physics)
