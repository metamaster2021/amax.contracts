#! /bin/bash

echo "Copy contract files..."
CONTRACTS_SRC_DIR="${BUILD_DIR}/contracts"
CONTRACTS_DEST_DIR="./contracts"

rm -r "${CONTRACTS_DEST_DIR}"

FILES=$( cd ${CONTRACTS_SRC_DIR} && find . -type f \( -name "*.wasm" -o -name "*.abi" \) | grep -v ./CMakeFiles )

for F in ${FILES[*]} 
do
   mkdir -p $(dirname "${CONTRACTS_DEST_DIR}/${F}")
   cp -v "${CONTRACTS_SRC_DIR}/${F}" "${CONTRACTS_DEST_DIR}/${F}"
done

echo 'Done update test contracts.'

RELEASE="${VERSION_SUFFIX}"

# default release to "1" if there is no suffix
if [[ -z $RELEASE ]]; then
  RELEASE="1"
fi

NAME="${PROJECT}_${VERSION_NO_SUFFIX}-${RELEASE}"

echo "Generating Tarball $NAME.tar.gz..."

tar -cvzf $NAME.tar.gz ${CONTRACTS_DEST_DIR} || exit 1

rm -r "${CONTRACTS_DEST_DIR}"
