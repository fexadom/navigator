_envsetup_load() {
  # Allow the environment to override the BDK path.
  local bdk="${BDK_PATH:-/home/federico/brillo_stuff/bdk}"
  if ! test -e "${bdk}/build/core/main.mk"; then
    echo "The BDK cannot be found." 1>&2
    echo "Please supply its path in the BDK_PATH environment variable." 1>&2
    return 1
  fi

  # Make sure we're in the root of a product.
  # If the user explicitly exported a PRODUCT_PATH, then fail if it is wrong.
  if test -n "${PRODUCT_PATH:-}" &&      ! test -e "${PRODUCT_PATH}/AndroidProducts.mk"; then
    echo "Cannot find a product at PRODUCT_PATH: ${PRODUCT_PATH}" 1>&2
    return 1
  fi
  # Otherwise, use the default or check the cwd.
  local product_path="${PRODUCT_PATH:-navigator}"
  if ! test -e "${product_path}/AndroidProducts.mk"; then
    echo "Checking current directory for a valid product . . ." 1>&2
    product_path=$PWD
    if ! test -e "${product_path}/AndroidProducts.mk"; then
      echo "Please source envsetup.sh from the product directory " 1>&2
      echo "or set a valid PRODUCT_PATH in the environment." 1>&2
      return 1
    fi
  fi

  # Find a free file descriptor and use it rather than a tempfile.
  local bdk_tools="${BDK_TOOLS_PATH:-/home/federico/brillo_stuff/bdk/tools/bdk}"
  fd=$((`cd /dev/fd && printf -- '%s
' * | sort -n | tail -1` + 1))
  eval 'exec '$fd'<<EOFEOF
  $(${bdk_tools}/brunch/brunch product envsetup \
    --product_path="${product_path}")
EOFEOF'
  . /dev/fd/$fd
  eval "exec $fd<&-"
}
_envsetup_load
