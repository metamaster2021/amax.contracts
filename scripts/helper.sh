# Ensures passed in version values are supported.
function check-version-numbers() {
  CHECK_VERSION_MAJOR=$1
  CHECK_VERSION_MINOR=$2

  if [[ $CHECK_VERSION_MAJOR -lt $AMAX_MIN_VERSION_MAJOR ]]; then
    exit 1
  fi
  if [[ $CHECK_VERSION_MAJOR -gt $AMAX_MAX_VERSION_MAJOR ]]; then
    exit 1
  fi
  if [[ $CHECK_VERSION_MAJOR -eq $AMAX_MIN_VERSION_MAJOR ]]; then
    if [[ $CHECK_VERSION_MINOR -lt $AMAX_MIN_VERSION_MINOR ]]; then
      exit 1
    fi
  fi
  if [[ $CHECK_VERSION_MAJOR -eq $AMAX_MAX_VERSION_MAJOR ]]; then
    if [[ $CHECK_VERSION_MINOR -gt $AMAX_MAX_VERSION_MINOR ]]; then
      exit 1
    fi
  fi
  exit 0
}


# Handles choosing which AMAX directory to select when the default location is used.
function default-amax-directories() {
  REGEX='^[0-9]+([.][0-9]+)?$'
  ALL_AMAX_SUBDIRS=()
  if [[ -d ${HOME}/amax ]]; then
    ALL_AMAX_SUBDIRS=($(ls ${HOME}/amax | sort -V))
  fi
  for ITEM in "${ALL_AMAX_SUBDIRS[@]}"; do
    if [[ "$ITEM" =~ $REGEX ]]; then
      DIR_MAJOR=$(echo $ITEM | cut -f1 -d '.')
      DIR_MINOR=$(echo $ITEM | cut -f2 -d '.')
      if $(check-version-numbers $DIR_MAJOR $DIR_MINOR); then
        PROMPT_AMAX_DIRS+=($ITEM)
      fi
    fi
  done
  for ITEM in "${PROMPT_AMAX_DIRS[@]}"; do
    if [[ "$ITEM" =~ $REGEX ]]; then
      AMAX_VERSION=$ITEM
    fi
  done
}


# Prompts or sets default behavior for choosing AMAX directory.
function amax-directory-prompt() {
  if [[ -z $AMAX_DIR_PROMPT ]]; then
    default-amax-directories;
    echo 'No AMAX location was specified.'
    while true; do
      if [[ $NONINTERACTIVE != true ]]; then
        if [[ -z $AMAX_VERSION ]]; then
          echo "No default AMAX installations detected..."
          PROCEED=n
        else
          printf "Is AMAX installed in the default location: $HOME/amax/$AMAX_VERSION (y/n)" && read -p " " PROCEED
        fi
      fi
      echo ""
      case $PROCEED in
        "" )
          echo "Is AMAX installed in the default location?";;
        0 | true | [Yy]* )
          break;;
        1 | false | [Nn]* )
          if [[ $PROMPT_AMAX_DIRS ]]; then
            echo "Found these compatible AMAX versions in the default location."
            printf "$HOME/amax/%s\n" "${PROMPT_AMAX_DIRS[@]}"
          fi
          printf "Enter the installation location of AMAX:" && read -e -p " " AMAX_DIR_PROMPT;
          AMAX_DIR_PROMPT="${AMAX_DIR_PROMPT/#\~/$HOME}"
          break;;
        * )
          echo "Please type 'y' for yes or 'n' for no.";;
      esac
    done
  fi
  export AMAX_INSTALL_DIR="${AMAX_DIR_PROMPT:-${HOME}/amax/${AMAX_VERSION}}"
}


# Prompts or default behavior for choosing AMAX.CDT directory.
function cdt-directory-prompt() {
  if [[ -z $CDT_DIR_PROMPT ]]; then
    echo 'No AMAX.CDT location was specified.'
    while true; do
      if [[ $NONINTERACTIVE != true ]]; then
        printf "Is AMAX.CDT installed in the default location? /usr/local/amax.cdt (y/n)" && read -p " " PROCEED
      fi
      echo ""
      case $PROCEED in
        "" )
          echo "Is AMAX.CDT installed in the default location?";;
        0 | true | [Yy]* )
          break;;
        1 | false | [Nn]* )
          printf "Enter the installation location of AMAX.CDT:" && read -e -p " " CDT_DIR_PROMPT;
          CDT_DIR_PROMPT="${CDT_DIR_PROMPT/#\~/$HOME}"
          break;;
        * )
          echo "Please type 'y' for yes or 'n' for no.";;
      esac
    done
  fi
  export CDT_INSTALL_DIR="${CDT_DIR_PROMPT:-/usr/local/amax.cdt}"
}


# Ensures AMAX is installed and compatible via version listed in tests/CMakeLists.txt.
function amnod-version-check() {
  INSTALLED_VERSION=$(echo $($AMAX_INSTALL_DIR/bin/amnod --version))
  INSTALLED_VERSION_MAJOR=$(echo $INSTALLED_VERSION | cut -f1 -d '.' | sed 's/v//g')
  INSTALLED_VERSION_MINOR=$(echo $INSTALLED_VERSION | cut -f2 -d '.' | sed 's/v//g')

  if [[ -z $INSTALLED_VERSION_MAJOR || -z $INSTALLED_VERSION_MINOR ]]; then
    echo "Could not determine AMAX version. Exiting..."
    exit 1;
  fi

  if $(check-version-numbers $INSTALLED_VERSION_MAJOR $INSTALLED_VERSION_MINOR); then
    if [[ $INSTALLED_VERSION_MAJOR -gt $AMAX_SOFT_MAX_MAJOR ]]; then
      echo "Detected AMAX version is greater than recommended soft max: $AMAX_SOFT_MAX_MAJOR.$AMAX_SOFT_MAX_MINOR. Proceed with caution."
    fi
    if [[ $INSTALLED_VERSION_MAJOR -eq $AMAX_SOFT_MAX_MAJOR && $INSTALLED_VERSION_MINOR -gt $AMAX_SOFT_MAX_MINOR ]]; then
      echo "Detected AMAX version is greater than recommended soft max: $AMAX_SOFT_MAX_MAJOR.$AMAX_SOFT_MAX_MINOR. Proceed with caution."
    fi
  else
    echo "Supported versions are: $AMAX_MIN_VERSION_MAJOR.$AMAX_MIN_VERSION_MINOR - $AMAX_MAX_VERSION_MAJOR.$AMAX_MAX_VERSION_MINOR"
    echo "Invalid AMAX installation. Exiting..."
    exit 1;
  fi
}
