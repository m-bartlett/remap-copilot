# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O3l
LDFLAGS := -levdev -lpthread
MAKEFILE := $(firstword $(MAKEFILE_LIST))
MAKEFLAGS += --no-builtin-rules

BINARY         := remap-copilot
SOURCE_DIR     := src
BUILD_DIR      := build
PREFIX         ?= /usr/local
INSTALL_PREFIX := $(PREFIX)/bin
INSTALL_TARGET := $(addprefix $(INSTALL_PREFIX)/,$(BINARY))
TARGET         := $(BUILD_DIR)/$(BINARY)

SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(patsubst $(SOURCE_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)

LIBS := libevdev
LDFLAGS  := $(shell pkg-config --libs $(LIBS))
CFLAGS   := $(shell pkg-config --cflags $(LIBS)) $(USERDEFINES)


define SYSTEMD_SERVICE_TEMPLATE
[Unit]
Description=Remap Windows Copilot Key to Right Control
After=graphical-session.target
Wants=graphical-session.target

[Service]
Type=simple
ExecStart=$(INSTALL_TARGET)
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
endef
export SYSTEMD_SERVICE_TEMPLATE
SYSTEMD_SERVICE_INSTALL_PATH = $(HOME)/.config/systemd/user/$(BINARY).service



all: $(TARGET)

echo:
# 	@echo "$$SYSTEMD_SERVICE_TEMPLATE"
	@echo "$(SYSTEMD_SERVICE_INSTALL_PATH)"

$(BUILD_DIR):
	@mkdir -pv $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(CFLAGS) -c $< -o $@

$(OBJECTS): $(SOURCES) $(HEADERS) | $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^
	@echo "Build complete: $(TARGET)"

$(SYSTEMD_SERVICE_INSTALL_PATH):
	@if [ $$EUID == 0 ]; then \
		echo "Install systemd service without root privileges."; \
	 else \
		 mkdir -pv "$$(dirname $(SYSTEMD_SERVICE_INSTALL_PATH))"; \
		 echo "$$SYSTEMD_SERVICE_TEMPLATE" > "$(SYSTEMD_SERVICE_INSTALL_PATH)"; \
		 systemctl --user daemon-reload; \
		 echo "Installed $(SYSTEMD_SERVICE_INSTALL_PATH)"; \
		 echo -e "Try:\n\tsystemctl --user start $(BINARY)"; \
		 echo -e "\tsystemctl --user enable --now $(BINARY)"; \
	 fi

clean:
	@rm -rfv $(BUILD_DIR)

$(INSTALL_TARGET): $(TARGET)
	@install --mode=0755 $(TARGET) $(INSTALL_TARGET)
	@echo "Installed $(INSTALL_TARGET)"

target: $(INSTALL_TARGET)

install-systemd: $(INSTALL_TARGET) $(SYSTEMD_SERVICE_INSTALL_PATH)

uninstall-systemd:
	@if [ -f "$(SYSTEMD_SERVICE_INSTALL_PATH)" ]; then \
		systemctl --user stop $(BINARY); \
		systemctl --user disable --now $(BINARY); \
		rm -fv $(SYSTEMD_SERVICE_INSTALL_PATH); \
		systemctl --user daemon-reload; \
		echo "Uninstalled $(BINARY) systemd service."; \
	 else \
	 	echo "$(BINARY) systemd service not installed."; \
	 fi

uninstall: uninstall-systemd
	@rm -fv $(INSTALL_TARGET)
	@echo "Uninstalled $(INSTALL_TARGET)"


.PHONY: clean install install-systemd uninstall uninstall-systemd