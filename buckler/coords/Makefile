# nRF application makefile for the coordinate display (Buckler side).
# Lives outside the buckler repo, so it points at the repo with relative paths:
# this folder is ~/eecs149-ddr/buckler/coords, the repo is ~/buckler.
PROJECT_NAME = $(shell basename "$(realpath ./)")

# Configurations
NRF_IC = nrf52832
SDK_VERSION = 15
SOFTDEVICE_MODEL = s132

# Source and header files
APP_HEADER_PATHS += .
APP_SOURCE_PATHS += .
APP_SOURCES = $(notdir $(wildcard ./*.c))

# Path to the buckler repo's software folder
BUCKLER_SW = ../../../buckler/software

# Path to base of nRF52-base repo
NRF_BASE_DIR = $(BUCKLER_SW)/nrf52x-base/

# Include board Makefile (if any)
include $(BUCKLER_SW)/boards/buckler_revC/Board.mk

# Include main Makefile
include $(NRF_BASE_DIR)make/AppMakefile.mk
