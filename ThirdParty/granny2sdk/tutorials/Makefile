# Makes or cleans all tutorial projects buildable on linux

PROJECTS = basic_loading create_control error_reporting \
           extended_data heap_check replace_file_api replace_memory_api tree_walker transform_file custom_format\
           string_table rendering/opengl s3tc/opengl

.PHONY: all $(PROJECTS)

all: $(PROJECTS)

$(PROJECTS):
	$(MAKE) -C $@
