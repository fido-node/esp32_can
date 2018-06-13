#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp_can

COMPONENT_ADD_INCLUDEDIRS := components/include	\


include $(IDF_PATH)/make/project.mk
