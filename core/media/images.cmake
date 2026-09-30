# Converts every image in imgs/ into a C file sized for the board's screen,
# using tools/img2c.py. Used by both the pico-sdk and ESP-IDF builds.
#
#   rcat_generate_images(<out_var> <screen_size> <python>)
#
# Sets <out_var> to the list of generated .c files. They're created in the
# build folder and rebuilt when an image or the converter changes. Images
# that aren't listed in core/media/image_list.c are dropped by the linker.

set(RCAT_IMAGES_REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

function(rcat_generate_images out_var screen_size python)
    set(tool ${RCAT_IMAGES_REPO_ROOT}/tools/img2c.py)
    set(out_dir ${CMAKE_CURRENT_BINARY_DIR}/images_${screen_size})

    execute_process(COMMAND ${python} -c "import PIL"
                    RESULT_VARIABLE pillow_missing OUTPUT_QUIET ERROR_QUIET)
    if(pillow_missing)
        message(FATAL_ERROR "The image converter needs Pillow, which isn't installed for ${python}.\n"
                            "Install it with:\n  ${python} -m pip install pillow")
    endif()

    file(GLOB images CONFIGURE_DEPENDS
         ${RCAT_IMAGES_REPO_ROOT}/imgs/*.png ${RCAT_IMAGES_REPO_ROOT}/imgs/*.PNG
         ${RCAT_IMAGES_REPO_ROOT}/imgs/*.bmp ${RCAT_IMAGES_REPO_ROOT}/imgs/*.BMP
         ${RCAT_IMAGES_REPO_ROOT}/imgs/*.jpg ${RCAT_IMAGES_REPO_ROOT}/imgs/*.JPG
         ${RCAT_IMAGES_REPO_ROOT}/imgs/*.jpeg ${RCAT_IMAGES_REPO_ROOT}/imgs/*.JPEG
         ${RCAT_IMAGES_REPO_ROOT}/imgs/*.gif ${RCAT_IMAGES_REPO_ROOT}/imgs/*.GIF)

    set(generated "")
    set(names "")
    foreach(image ${images})
        # Same name rules as img2c.py: the file name, with anything that
        # isn't a letter, digit or underscore replaced by an underscore.
        get_filename_component(name ${image} NAME_WE)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" name "${name}")
        if(name IN_LIST names)
            message(FATAL_ERROR "Two images in imgs/ have the name '${name}'. Rename one of them.")
        endif()
        list(APPEND names ${name})

        set(out ${out_dir}/${name}.c)
        add_custom_command(OUTPUT ${out}
                           COMMAND ${python} ${tool} ${image} --size ${screen_size} --out ${out}
                           DEPENDS ${image} ${tool}
                           COMMENT "Converting image ${name} for a ${screen_size}x${screen_size} screen"
                           VERBATIM)
        list(APPEND generated ${out})
    endforeach()

    set(${out_var} ${generated} PARENT_SCOPE)
endfunction()
