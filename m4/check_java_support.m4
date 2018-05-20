AU_ALIAS([AC_CHECK_JAVA_VERSION], [AX_CHECK_JAVA_VERSION])
AU_ALIAS([AC_CHECK_GRADLE_VERSION], [AX_CHECK_GRADLE_VERSION])

AC_DEFUN([AX_READLINK],
[
  READLINK_TARGET=[$1]

  if test -n "$READLINK_TARGET"; then
    cd $(dirname "$READLINK_TARGET")
    while test -L "$READLINK_TARGET"; do
      READLINK_TARGET=$(readlink "$READLINK_TARGET")
      cd $(dirname "$READLINK_TARGET")
    done
    echo $(pwd -P)/$(basename "$READLINK_TARGET")
  fi
])
AC_DEFUN([AX_CHECK_GRADLE_VERSION],
[AC_MSG_CHECKING([for GRADLE_VERSION])
  EXPECTED_GRADLE_VERSION=[$1]
  GRADLE_BIN=`which gradle`
  if test "x$GRADLE_BIN" != "x"; then
    GRADLE_BIN=`AX_READLINK([$GRADLE_BIN])`
    GRADLE_VERSION=`gradle -version 2>&1 | grep Gradle | head -1 |sed "s/.*\ \(.*\)/\1/"`
    SHORT_VERSION=${GRADLE_VERSION%.*}
    MAJOR_VERSION=${SHORT_VERSION%.*}
    MINOR_VERSION=${SHORT_VERSION##*.}
    EXPECTED_MAJOR_VERSION=${EXPECTED_GRADLE_VERSION%.*}
    EXPECTED_MINOR_VERSION=${EXPECTED_GRADLE_VERSION##*.}
    if test "$MAJOR_VERSION" -lt "$EXPECTED_MAJOR_VERSION";
    then
      AC_MSG_ERROR([Too old gradle version required: $EXPECTED_GRADLE_VERSION found: $GRADLE_VERSION])
    elif test "$MAJOR_VERSION" -eq "$EXPECTED_MAJOR_VERSION";
    then
      if test "$MINOR_VERSION" -lt "$EXPECTED_MINOR_VERSION";
      then
        AC_MSG_ERROR([Too old gradle version required: $EXPECTED_GRADLE_VERSION= found: $GRADLE_VERSION])
      fi
    fi
    AC_SUBST(GRADLE, "$GRADLE_BIN")
    $2
    AC_MSG_RESULT([$SHORT_VERSION])
  else
    $3
    AC_MSG_RESULT([no])
  fi
])
AC_DEFUN([AX_CHECK_JAVA_VERSION],
[AC_MSG_CHECKING([for JAVA_VERSION])
  case $host_os in
    freebsd*) DONT_RESOLVE_JAVA_BIN_LINKS="YES" ;;
    *) ;;
  esac
  JAVA_VERSION=[$1]
  JAVAC_BIN=`which javac`
  JAVAH_BIN=`which javah`
  JAR_BIN=`which jar`
  JAVA_HOME_CHECKER="/usr/libexec/java_home"

  if test "x$JAVAC_BIN" != "x"; then
    if test "x$DONT_RESOLVE_JAVA_BIN_LINKS" == "x"; then
      JAVAC_BIN=`AX_READLINK([$JAVAC_BIN])`
      JAVAH_BIN=`AX_READLINK([$JAVAH_BIN])`
      JAR_BIN=`AX_READLINK([$JAR_BIN])`
    fi
    JAVAC_VERSION=`$JAVAC_BIN -version 2>&1 | sed "s/.*\ \(.*\)/\1/"`
    SHORT_VERSION=${JAVAC_VERSION%.*}
    MAJOR_VERSION=${SHORT_VERSION%.*}
    MINOR_VERSION=${SHORT_VERSION##*.}
    VERSION_OK="1"

    EXPECTED_MAJOR_VERSION=${JAVA_VERSION%.*}
    EXPECTED_MINOR_VERSION=${JAVA_VERSION##*.}
    if test "$MAJOR_VERSION" -lt "$EXPECTED_MAJOR_VERSION";
    then
      AC_MSG_NOTICE([Too old java version required: $JAVA_VERSION found: $SHORT_VERSION])
      VERSION_OK="0"
    elif test "$MAJOR_VERSION" -eq "$EXPECTED_MAJOR_VERSION";
    then
      if test "$MINOR_VERSION" -lt "$EXPECTED_MINOR_VERSION";
      then
        AC_MSG_NOTICE([Too old java version required: $JAVA_VERSION found: $SHORT_VERSION])
        VERSION_OK="0"
      fi
    fi

    if test "$VERSION_OK" = "1";
    then
      if test -e "$JAVA_HOME_CHECKER"; then
        JNI_HOME=`$JAVA_HOME_CHECKER`
      else
        JNI_HOME=`echo $JAVAC_BIN | sed "s/\(.*\)[[/]]bin[[/]]java.*/\1\//"`
      fi

      JNI_LIBDIR=`find $JNI_HOME \( -name "libjvm.so" -or -name "libjvm.dylib" \) \
        | sed "s-/libjvm\.so-/-" \
        | sed "s-/libjvm\.dylib-/-" | head -n 1`
      JNI_LIBS="-L$JNI_LIBDIR -ljvm"
      JNI_INCLUDE_DIR=`find $JNI_HOME -name "jni.h" |  sed "s/\(.*\)jni.h/\1/" | head -n 1`
      JNI_CFLAGS="-I$JNI_INCLUDE_DIR"

      JNI_MD_INCLUDE_DIR=`find $JNI_HOME -name "jni_md.h" |  sed "s/\(.*\)jni_md.h/\1/" | head -n 1`
      JNI_CFLAGS="$JNI_CFLAGS -I$JNI_MD_INCLUDE_DIR"
      AC_SUBST(JNI_CFLAGS, "$JNI_CFLAGS")
      AC_SUBST(JNI_LIBS, "$JNI_LIBS")
      AC_SUBST(JAVAC, "$JAVAC_BIN")
      AC_SUBST(JAVAH, "$JAVAH_BIN")
      AC_SUBST(JAR, "$JAR_BIN")
      $2
      AC_MSG_RESULT([$SHORT_VERSION])
    else
        $3
        AC_MSG_RESULT([no])
    fi
  else
    $3
    AC_MSG_RESULT([no])
  fi
])
