AU_ALIAS([AC_CHECK_JAVA_VERSION], [AX_CHECK_JAVA_VERSION])

AC_DEFUN([AX_CHECK_JAVA_VERSION],
[AC_MSG_CHECKING([for JAVA_VERSION])
  JAVA_VERSION=[$1]
  JAVAC_BIN=`which javac`
  JAVAH_BIN=`which javah`
  JAR_BIN=`which jar`
  if test "x$JAVAC_BIN" != "x"; then
    JAVAC_BIN=`readlink -f $JAVAC_BIN`
    JAVAH_BIN=`readlink -f $JAVAH_BIN`
    JAR_BIN=`readlink -f $JAR_BIN`
    JAVAC_VERSION=`$JAVAC_BIN -version 2>&1 | sed "s/.*\ \(.*\)/\1/"`
    SHORT_VERSION=${JAVAC_VERSION%.*}
    MAJOR_VERSION=${SHORT_VERSION%.*}
    MINOR_VERSION=${SHORT_VERSION##*.}

    EXPECTED_MAJOR_VERSION=${JAVA_VERSION%.*}
    EXPECTED_MINOR_VERSION=${JAVA_VERSION##*.}
    if test "$MAJOR_VERSION" -lt "$EXPECTED_MAJOR_VERSION";
    then
      AC_MSG_ERROR([Too old java version required: $JAVA_VERSION found: $SHORT_VERSION])
    elif test "$MAJOR_VERSION" -eq "$EXPECTED_MAJOR_VERSION";
    then
      if test "$MINOR_VERSION" -lt "$EXPECTED_MINOR_VERSION";
      then
        AC_MSG_ERROR([Too old java version required: $JAVA_VERSION found: $SHORT_VERSION])
      fi
    fi

    JNI_HOME=`echo $JAVAC_BIN | sed "s/\(.*\)[[/]]bin[[/]]java.*/\1/"`
    JNI_LIBDIR=`find $JNI_HOME -name "libjvm.so" | sed "s/\(.*\)libjvm.so/\1/" | head -n 1`
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
])
