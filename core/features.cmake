# Optional features: the sections of the UI you move between by swiping up and
# down. Each platform picks which ones to build in its features.cmake, by
# setting RCAT_FEATURE_<NAME> to ON or OFF. A feature that isn't set there is
# built in. Settings is always built in.
#
# Feature <NAME> is the file ui/feature_<name>.c. To add one, add its name
# here, add it to the list in ui/ui.c, and add it to each platform's
# features.cmake.

set(RCAT_FEATURES IMAGES VOICE SOUND LEVEL LOVENSE)

# Leave the source files of switched off features out of the list in
# sources_var.
function(rcat_feature_filter_sources sources_var)
    set(sources ${${sources_var}})
    foreach(feature ${RCAT_FEATURES})
        if(DEFINED RCAT_FEATURE_${feature} AND NOT RCAT_FEATURE_${feature})
            string(TOLOWER ${feature} name)
            list(FILTER sources EXCLUDE REGEX "/ui/feature_${name}\\.c$")
        endif()
    endforeach()
    set(${sources_var} ${sources} PARENT_SCOPE)
endfunction()

# Compile definitions telling the code which features are built in:
# RCAT_FEATURE_<NAME>=1 or 0.
function(rcat_feature_definitions definitions_var)
    set(definitions "")
    set(enabled "")
    foreach(feature ${RCAT_FEATURES})
        if(NOT DEFINED RCAT_FEATURE_${feature} OR RCAT_FEATURE_${feature})
            list(APPEND definitions RCAT_FEATURE_${feature}=1)
            list(APPEND enabled ${feature})
        else()
            list(APPEND definitions RCAT_FEATURE_${feature}=0)
        endif()
    endforeach()
    if(enabled)
        string(REPLACE ";" ", " enabled "${enabled}")
    else()
        set(enabled "none, only settings")
    endif()
    message(STATUS "Features: ${enabled}")
    set(${definitions_var} ${definitions} PARENT_SCOPE)
endfunction()
