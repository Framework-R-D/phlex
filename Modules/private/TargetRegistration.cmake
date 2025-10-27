#
# Defines a generic mechanism for registering build targets for different
# tool integrations (e.g., clang-tidy, code coverage).
#

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# phlex_add_target_for_tool
#
# Registers a target to be included in the analysis for a specific tool.
#
# @param TOOL_NAME The name of the tool (e.g., CLANG_TIDY, COVERAGE).
#                  This will be used to construct the name of a global
#                  property that stores the list of targets.
# @param TARGET    The name of the build target to register.
#
function(phlex_add_target_for_tool TOOL_NAME TARGET)
  # Define the global property name based on the tool name
  set(PROPERTY_NAME "PHLEX_TOOL_TARGETS_${TOOL_NAME}")

  # Append the target to the global list stored in the property
  set_property(GLOBAL APPEND PROPERTY ${PROPERTY_NAME} ${TARGET})
endfunction()
