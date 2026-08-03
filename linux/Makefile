CC ?= cc
CFLAGS ?= -O2 -pipe
CFLAGS += -std=c17 -Wall -Wextra -Wno-format-truncation -DNOTEMD_DEV_DATA_DIR='"$(abspath .)"'
LDFLAGS += -ldl

.PHONY: all clean run install-user uninstall-user deb

all: build/notemd

build/notemd: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

run: build/notemd
	./build/notemd

install-user: build/notemd
	install -Dm755 build/notemd $(HOME)/.local/bin/notemd
	install -Dm644 packaging/pt.notemd.NoteMD.desktop $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 ../Assets/NoteMD-AppIcon-1024.png $(HOME)/.local/share/icons/hicolor/1024x1024/apps/pt.notemd.NoteMD.png
	install -Dm644 ../Assets/NoteMD-AppIcon-1024.png $(HOME)/.local/share/notemd/NoteMD-AppIcon-1024.png
	sed -i 's|^Exec=.*|Exec=$(HOME)/.local/bin/notemd %F|' $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	sed -i 's|^Icon=.*|Icon=$(HOME)/.local/share/icons/hicolor/1024x1024/apps/pt.notemd.NoteMD.png|' $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 style.css $(HOME)/.local/share/notemd/style.css
	install -Dm644 themes/system.css $(HOME)/.local/share/notemd/themes/system.css
	install -Dm644 themes/white.css $(HOME)/.local/share/notemd/themes/white.css
	install -Dm644 themes/black.css $(HOME)/.local/share/notemd/themes/black.css
	install -Dm644 themes/monokai.css $(HOME)/.local/share/notemd/themes/monokai.css
	install -Dm644 themes/tokyo-night.css $(HOME)/.local/share/notemd/themes/tokyo-night.css
	install -Dm644 themes/dracula.css $(HOME)/.local/share/notemd/themes/dracula.css
	install -Dm644 themes/solarized-light.css $(HOME)/.local/share/notemd/themes/solarized-light.css
	install -Dm644 packaging/pt.notemd.NoteMD.metainfo.xml $(HOME)/.local/share/metainfo/pt.notemd.NoteMD.metainfo.xml

uninstall-user:
	rm -f $(HOME)/.local/bin/notemd
	rm -f $(HOME)/.local/share/applications/pt.notemd.NoteMD.desktop
	rm -f $(HOME)/.local/share/icons/hicolor/1024x1024/apps/pt.notemd.NoteMD.png
	rm -rf $(HOME)/.local/share/notemd
	rm -f $(HOME)/.local/share/metainfo/pt.notemd.NoteMD.metainfo.xml

clean:
	rm -f build/notemd

deb: build/notemd
	rm -rf build/deb
	install -Dm755 build/notemd build/deb/usr/bin/notemd
	install -Dm644 packaging/debian-control build/deb/DEBIAN/control
	install -Dm644 packaging/pt.notemd.NoteMD.desktop build/deb/usr/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 packaging/pt.notemd.NoteMD.metainfo.xml build/deb/usr/share/metainfo/pt.notemd.NoteMD.metainfo.xml
	install -Dm644 ../Assets/NoteMD-AppIcon-1024.png build/deb/usr/share/icons/hicolor/1024x1024/apps/pt.notemd.NoteMD.png
	install -Dm644 ../Assets/NoteMD-AppIcon-1024.png build/deb/usr/share/notemd/NoteMD-AppIcon-1024.png
	sed -i 's|^Exec=.*|Exec=/usr/bin/notemd %F|' build/deb/usr/share/applications/pt.notemd.NoteMD.desktop
	sed -i 's|^Icon=.*|Icon=/usr/share/icons/hicolor/1024x1024/apps/pt.notemd.NoteMD.png|' build/deb/usr/share/applications/pt.notemd.NoteMD.desktop
	install -Dm644 style.css build/deb/usr/share/notemd/style.css
	install -Dm644 themes/system.css build/deb/usr/share/notemd/themes/system.css
	install -Dm644 themes/white.css build/deb/usr/share/notemd/themes/white.css
	install -Dm644 themes/black.css build/deb/usr/share/notemd/themes/black.css
	install -Dm644 themes/monokai.css build/deb/usr/share/notemd/themes/monokai.css
	install -Dm644 themes/tokyo-night.css build/deb/usr/share/notemd/themes/tokyo-night.css
	install -Dm644 themes/dracula.css build/deb/usr/share/notemd/themes/dracula.css
	install -Dm644 themes/solarized-light.css build/deb/usr/share/notemd/themes/solarized-light.css
	dpkg-deb --build --root-owner-group build/deb build/notemd_1.2.0_amd64.deb
