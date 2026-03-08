pkgname=gtkdecor
pkgver=0.2.1
pkgrel=1
pkgdesc="GTK3-themed server-side decorations for Wayfire"
arch=('x86_64')
license=('MIT')
depends=(
  'wayfire'
  'cairo'
  'pango'
  'librsvg'
)
makedepends=('meson' 'ninja')
source=()

build() {
  cd "$startdir"
  meson setup builddir --prefix=/usr --buildtype=plain --wipe
  ninja -C builddir
}

package() {
  cd "$startdir"
  DESTDIR="$pkgdir" meson install -C builddir --no-rebuild
}
