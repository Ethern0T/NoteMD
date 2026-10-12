CC ?= cc
CFLAGS ?= -O2 -pipe
CFLAGS += -std=c17 -Wall -Wextra -Wmissing-prototypes -Wno-format-truncation -Werror=implicit-function-declaration \
	-ffunction-sections -fdata-sections -DNOTEMD_DEV_DATA_DIR='"$(abspath .)"'
LDFLAGS += -Wl,--gc-sections -ldl
ICON_SIZES = 32 48 64 128 256 512 1024
VERSION := $(shell cat ../VERSION)

.PHONY: all clean check run install-user uninstall-user deb

all: build/notemd

SOURCES = src/main.c src/core.c src/library.c src/export.c src/editor.c src/ui.c src/tests.c
OBJECTS = $(patsubst src/%.c,build/%.o,$(SOURCES))
HEADERS = src/internal.h src/markdown.h src/pdf.h src/preview.h src/source-editor.h src/markdown-tests.h src/vendor/md4c-abi.h

build/notemd: $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

build/%.o: src/%.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

check: build/notemd
	./build/notemd --self-test
	./build/notemd --settings-self-test
	./build/notemd --export-self-test
	./build/notemd --markdown-self-test

run: build/notemd
	./build/notemd

install-user: build/notemd
	install -Dm755 build/notemd $(HOME)/.local/bin/notemd
	install -Dm644 packaging/pt.notemd.NoteMD.desktop $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 assets/pt.notemd.NoteMD.svg $(HOME)/.local/share/icons/hicolor/scalable/apps/pt.notemd.NoteMD.svg
	@for icon_size in $(ICON_SIZES); do install -Dm644 assets/icons/$${icon_size}x$${icon_size}/pt.notemd.NoteMD.png "$(HOME)/.local/share/icons/hicolor/$${icon_size}x$${icon_size}/apps/pt.notemd.NoteMD.png"; done
	install -Dm644 assets/icons/1024x1024/pt.notemd.NoteMD.png $(HOME)/.local/share/notemd/NoteMD-AppIcon-1024.png
	sed -i 's|^Exec=.*|Exec=$(HOME)/.local/bin/notemd %F|' $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 style.css $(HOME)/.local/share/notemd/style.css
	install -Dm644 packaging/md4c-license.txt $(HOME)/.local/share/notemd/licenses/md4c.txt
	@for syntax_file in syntax/*; do install -Dm644 "$$syntax_file" "$(HOME)/.local/share/notemd/$$syntax_file"; done
	install -Dm644 assets/notemd-insert-table-symbolic.svg $(HOME)/.local/share/notemd/assets/notemd-insert-table-symbolic.svg
	install -Dm644 themes/system.css $(HOME)/.local/share/notemd/themes/system.css
	install -Dm644 themes/white.css $(HOME)/.local/share/notemd/themes/white.css
	install -Dm644 themes/black.css $(HOME)/.local/share/notemd/themes/black.css
	install -Dm644 themes/monokai.css $(HOME)/.local/share/notemd/themes/monokai.css
	install -Dm644 themes/tokyo-night.css $(HOME)/.local/share/notemd/themes/tokyo-night.css
	install -Dm644 themes/dracula.css $(HOME)/.local/share/notemd/themes/dracula.css
	install -Dm644 themes/solarized-light.css $(HOME)/.local/share/notemd/themes/solarized-light.css
	install -Dm644 themes/catppuccin-latte.css $(HOME)/.local/share/notemd/themes/catppuccin-latte.css
	install -Dm644 packaging/pt.notemd.NoteMD.metainfo.xml $(HOME)/.local/share/metainfo/pt.notemd.NoteMD.metainfo.xml
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache -f -t "$(HOME)/.local/share/icons/hicolor"; fi

uninstall-user:
	rm -f $(HOME)/.local/bin/notemd
	rm -f $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	rm -f $(foreach size,$(ICON_SIZES),$(HOME)/.local/share/icons/hicolor/$(size)x$(size)/apps/pt.notemd.NoteMD.png)
	rm -f $(HOME)/.local/share/icons/hicolor/scalable/apps/pt.notemd.NoteMD.svg
	rm -rf $(HOME)/.local/share/notemd
	rm -f $(HOME)/.local/share/metainfo/pt.notemd.NoteMD.metainfo.xml

clean:
	rm -f build/notemd build/*.o build/notemd.aux

deb: build/notemd
	rm -rf build/deb
	install -Dm755 build/notemd build/deb/usr/bin/notemd
	install -Dm644 packaging/debian-control build/deb/DEBIAN/control
	sed -i 's/^Version:.*/Version: $(VERSION)/' build/deb/DEBIAN/control
	install -Dm644 packaging/pt.notemd.NoteMD.desktop build/deb/usr/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 packaging/pt.notemd.NoteMD.metainfo.xml build/deb/usr/share/metainfo/pt.notemd.NoteMD.metainfo.xml
	install -Dm644 assets/pt.notemd.NoteMD.svg build/deb/usr/share/icons/hicolor/scalable/apps/pt.notemd.NoteMD.svg
	@for icon_size in $(ICON_SIZES); do install -Dm644 assets/icons/$${icon_size}x$${icon_size}/pt.notemd.NoteMD.png "build/deb/usr/share/icons/hicolor/$${icon_size}x$${icon_size}/apps/pt.notemd.NoteMD.png"; done
	install -Dm644 assets/icons/1024x1024/pt.notemd.NoteMD.png build/deb/usr/share/notemd/NoteMD-AppIcon-1024.png
	sed -i 's|^Exec=.*|Exec=/usr/bin/notemd %F|' build/deb/usr/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 style.css build/deb/usr/share/notemd/style.css
	install -Dm644 packaging/md4c-license.txt build/deb/usr/share/doc/notemd/md4c.txt
	@for syntax_file in syntax/*; do install -Dm644 "$$syntax_file" "build/deb/usr/share/notemd/$$syntax_file"; done
	install -Dm644 assets/notemd-insert-table-symbolic.svg build/deb/usr/share/notemd/assets/notemd-insert-table-symbolic.svg
	install -Dm644 themes/system.css build/deb/usr/share/notemd/themes/system.css
	install -Dm644 themes/white.css build/deb/usr/share/notemd/themes/white.css
	install -Dm644 themes/black.css build/deb/usr/share/notemd/themes/black.css
	install -Dm644 themes/monokai.css build/deb/usr/share/notemd/themes/monokai.css
	install -Dm644 themes/tokyo-night.css build/deb/usr/share/notemd/themes/tokyo-night.css
	install -Dm644 themes/dracula.css build/deb/usr/share/notemd/themes/dracula.css
	install -Dm644 themes/solarized-light.css build/deb/usr/share/notemd/themes/solarized-light.css
	install -Dm644 themes/catppuccin-latte.css build/deb/usr/share/notemd/themes/catppuccin-latte.css
	dpkg-deb --build --root-owner-group build/deb build/notemd_$(VERSION)_amd64.deb
