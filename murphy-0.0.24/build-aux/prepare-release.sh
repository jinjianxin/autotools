#!/bin/bash

# This needs a bit more work, mostly on the "discplined engineering" front.
# IOW, instead of this UPSTREAM_BASE hack it would be better to have 3
# branches:
#   1) pristine upstream: for tracking upstream progress/retrogression
#   2) patched upstream: pristine upstream with our patches applied
#   3) working local: patches upstream + a set of scripts (like this) to
#      do everyday stuff like making new releases, exporting stuff to
#      OBS, etc...

# show help on usage and exit
usage() {
    echo ""
    echo "usage: $0 [options]"
    echo "The possible options are"
    echo "  -n <name>             name of the package"
    echo "  -v <version>          version to give to this release"
    echo "  --release <gitrev>    git branch, tag or SHA1 to release"
    echo "  --baseline <base>     use <base> as baseline for release"
    echo "  --obs                 prepare for OBS release"
    echo "  --gerrit-review       prepare for gerrit review"
    echo "  --gerrit-release      prepare for gerrit release"
    echo ""
    echo "<name> is the name of the package. <version> is the version you want"
    echo "to give to this release in RPM. <gitrev> is the actual git version"
    echo "(typically tag, branch, or SHA1) you want to release. If <base> is"
    echo "specified the release will contain a set of patches on top of this"
    echo "baseline. Otherwise no patches will be generated. If --obs is given"
    echo "the relase will be generated in a format (spec, tarball + patches)"
    echo "suitable for importing to OBS. If --gerrit-review or --gerrit-release"
    echo "is used, the release will be generated as a branch suitable to be"
    echo "pushed to gerrit for review or review-bypassing release."
    echo ""
    echo "Examples:"
    echo "Prepare a branch of 'HEAD' for gerrit review as 0.0.9:"
    echo "    $0 --release HEAD -v 0.0.9 --gerrit-review"
    echo ""
    echo "Prepare a branch of 'tizen' for gerrit release as 0.10.1:"
    echo "    $0 --release tizen -v 0.10.1 --gerrit-release"
    echo ""
    echo "Prepare directory obs-0.5.0 from 'foobar' suitable for exporting"
    echo "to OBS as version 0.5.0:"
    echo "    $0 --release foobar -v 0.5.0 --baseline foo"
    echo ""
    echo "<name> defaults to $(basename $(pwd))"
    echo "<release> defaults to HEAD"
    echo "<version> defaults to $(default_version)"
    echo ""
    exit 0
}

# emit an error message to stderr
error() {
    echo "error: $*" 1>&2
}

# emit an info message to stdout
info() {
    echo "$*"
}

# emit a warning message to stderr
warning() {
    echo "warning: $*" 1>&2
}

# indent piped output
indent() {
    local _prefix="${1:-    }"

    sed "s/^/$_prefix/g"
}

# record an undo action
record_undo() {
    if [ -n "$UNDO" ]; then
        UNDO="$*;$UNDO"
    else
        UNDO="$*"
    fi
}

# execute recorded undo actions
execute_undo() {
    if [ -n "$UNDO" ]; then
        echo "Executing recorded undo actions."
        eval $UNDO
    fi

    echo "Exiting due to errors."
    exit 1
}

# enable automatic undo
enable_auto_undo() {
    trap undo_execute ERR
    set -e
    set -o pipefail
}

# generate default version number
default_version() {
    echo "$(date +'%Y%m%d')"
}

# generate a tarball for a given package and version from git to a directory
generate_tarball() {
    local _pkg="$1" _pkgversion="$2" _gitversion="$3" _dir="$4"
    local _tarball="$_dir/$_pkg-$_pkgversion.tar"

    info "* Generating tarball $_tarball.gz..."
    git archive --format=tar --prefix=$_pkg-$_pkgversion/ \
        $_gitversion > $_tarball && \
    gzip $_tarball
}

# generate patches to a directory against a baseline, print list of patches
generate_patches() {
    local _baseline="$1" _head="$2" _dir="$3" _patchret="$4" _patchlist

    info "* Generating patches from $_baseline till $_head..." 1>&2
    pushd $_dir >& /dev/null && \
        _patchlist="`git format-patch -n $_baseline..$_head`" && \
    popd >& /dev/null

    if [ -n "$_patchret" ]; then
        eval "$_patchret=\"$_patchlist\""
    else
        echo "$_patchlist"
    fi
}

# generate a spec file from a spec template filling in version and patches
generate_specfile() {
    local _specin="$1" _specout="$2" _version="$3" _patchlist

    shift 3
    _patchlist="$*"

    info "* Generating spec file $_specout (from $_specin)..."

    cat $_specin | sed "s/@VERSION@/$_version/g" > $_specout.tmp && \
    cat $_specout.tmp | while read -r line; do
        case $line in
        @DECLARE_PATCHES@)
            i=0
            for patch in $_patchlist; do
                echo "Patch$i: $patch"
                let i=$i+1
            done
            ;;
        @APPLY_PATCHES@)
            i=0
            for patch in $_patchlist; do
                echo "%patch$i -p1"
                let i=$i+1
            done
            ;;
        *)
            echo "$line"
            ;;
        esac
    done > $_specout && \
    rm -f $_specout.tmp
}

# generate a changelog for the release (with no real changelog content)
generate_changelog() {
    local _chlog="$1" _version="$2" _gitver="$3" _author="$4"
    local _sha

    info "* Generating changelog $_chlog for version $_version ($_gitver)..."

    _sha="`git rev-parse $_gitver`"

    echo "* $(date '+%a %b %d %H:%M:%S %Z %Y') $_author - $_version" > $_chlog
    echo "- release: released $_version (git: $_sha)." >> $_chlog
}

# generate linker scripts
generate_linker_scripts() {
    info "* Generating linker scripts..."
    ./bootstrap && ./configure && make generate-linker-scripts
}

# generate a release branch
generate_branch() {
    local _br="$1" _rel="$2" _current

    info "* Creating release branch $_br from $_rel..."

    _current="`git_current_branch`"

    git branch $_br $_rel
    record_undo "git branch -D $_br"

    git checkout $_br
    record_undo "git checkout $_current"
}

# add files/directories to the repository
git_add() {
    local _msg="$1"
    shift

    git add $*
    git commit -n -m "$_msg" $*
}

# add an annotated git tag
git_tag() {
    local _tag="$1" _msg="$2"

    info "* Tagging for release..."

    git tag -a -m "$_msg" $_tag
    record_undo "git branch -D $_tag"
}

# run package-specific GBS quirks
package_gbs_quirks() {
    local _ld_scripts
    local _pkg_quirks

    _pkg_quirks="${0%/prepare-release.sh}/gbs-quirks"
    if [ -f $_pkg_quirks ]; then
        info "* Running package-specific GBS quirks ($_pkg_quirks)..."
        source $_pkg_quirks
    else
        info "* No package-specific GBS quirks found ($_pkg_quirks)..."
    fi
}

# parse the command line for configuration
parse_configuration() {
    while [ -n "${1#-}" ]; do
        case $1 in
            --name|-n)
                if [ -z "$PKG" ]; then
                    shift
                    PKG="$1"
                else
                    error "Multiple package names ($PKG, $1) specified."
                    usage
                fi
                ;;
            --version|-v)
                if [ -z "$VERSION" ]; then
                    shift
                    VERSION="$1"
                else
                    error "Multiple versions ($VERSION, $1) specified."
                    usage
                fi
                ;;
             --release|-R|--head|-H)
                if [ -z "$RELEASE" ]; then
                    shift
                    RELEASE="$1"
                else
                    error "Multiple git versions ($RELEASE, $1) specified."
                    usage
                fi
                ;;
             --base|--baseline|-B)
                if [ -z "$BASELINE" ]; then
                    shift
                    BASELINE="$1"
                else
                    error "Multiple git baselines ($BASELINE, $1) specified."
                    usage
                fi
                ;;
             --obs|-O)
                OBS="yes"
                ;;
             --gbs*)
                if [ -z "$GBS" ]; then
                    TYPE="${1#--gbs-}"
                    GBS="yes"
                else
                    error "Multiple --gbs options specified."
                    usage
                fi
                ;;
             --gerrit*|-G)
                if [ -z "$GBS" ]; then
                    TYPE="${1#--gerrit-}"
                    GBS="yes"
                else
                    error "Multiple --gerrit options specified."
                    usage
                fi
                ;;
             --target|-T)
                if [ -z "$TARGET" ]; then
                    shift
                    TARGET="$1"
                else
                    error "Multiple targets ($TARGET, $1) specified."
                    usage
                fi
                ;;
             --author|-A)
                if [ -z "$AUTHOR" ]; then
                    shift
                    AUTHOR="$1"
                else
                    error "Multiple authors ($AUTHOR, $1) specified."
                    usage
                fi
                ;;
             --help|-h)
                usage 0
                ;;
             --debug|-d)
                set -x
                DEBUG=$((${DEBUG:-0} + 1))
                ;;
             *)
                error "Unknown option or unexpected argument '$1'."
                usage
                ;;
        esac
        shift
    done
}

# determine current git branch
git_current_branch() {
    local _br

    _br="`git branch -l | grep '^[^ ]' | cut -d ' ' -f 2`"

    echo "$_br"
}

# prepare for gbs
gbs_prepare() {
    local _dir _specin _specout _chlog _patches
    local _stamp _branch _tag _remote

    _dir=packaging
    _specin=packaging.in/$PKG.spec.in
    _specout=$_dir/$PKG.spec
    _chlog=$_dir/$PKG.changes
    _stamp=$(date -u +%Y%m%d.%H%M%S)
    _branch=release/$TARGET/$_stamp
    _tag=submit/$TARGET/$_stamp

    case $TYPE in
        release)  _remote=refs/heads/$TARGET;;
        review|*) _remote=refs/for/$TARGET;;
    esac

    mkdir -p $_dir
    record_undo "rm -fr $TOPDIR/$_dir"

    if [ -z "$BASELINE" ]; then
        _base=$RELEASE
        _patches=""
    else
        _base=$BASELINE
        generate_patches $_base $RELEASE $_dir _patches
        echo "$_patches" | tr -s '\t' ' ' | tr ' ' '\n' | indent "PATCHES: "
    fi

    generate_specfile $_specin $_specout $VERSION $_patches | indent "GENSPEC: "

    generate_branch $_branch $_base | indent " BRANCH: "

    package_gbs_quirks 2>&1 | indent "QUIRKS: "

    generate_changelog $_chlog $VERSION $RELEASE "$AUTHOR" | indent " GENLOG: "

    git_add "packaging: added packaging." $_dir | indent " GITADD: "

    git_tag $_tag "Tagged $_stamp ($VERSION) for release to $TARGET." | \
        indent "GITTAG: "

    info ""
    info "Branch $_branch prepared for release. Please check if"
    info "everything looks okay. If it does, you can proceed with the"
    info "release by executing the following commands:"
    info ""
    info "    git push <remote-repo> $_branch:$_remote"
    info "    git push <remote-repo> $_tag"
    info ""
}

# export to OBS
obs_export() {
    local _patches _dir _specin _specout

    _dir=obs-$VERSION
    _specin=packaging.in/$PKG.spec.in
    _specout=$_dir/$PKG.spec

    mkdir -p $_dir
    rm -f $_dir/*.spec $_dir/*.tar.gz $_dir/*.patches

    if [ -n "$BASELINE" ]; then
        _base=$BASELINE
        _patches="`generate_patches $_base $RELEASE $_dir`"
    else
        _base=$RELEASE
        _patches=""
    fi

    generate_specfile $_specin $_specout $VERSION $_patches
    generate_tarball $PKG $VERSION $_base $_dir
}


#########################
# main script

if [ ! -d .git ]; then
    error "Please always run ${0##*/} from the top directory."
    exit 1
else
    TOPDIR=$(basename $(pwd))
fi

parse_configuration $*

[ -z "$PKG" ]     && PKG=$(basename `pwd`)
[ -z "$RELEASE" ] && RELEASE=HEAD
[ -z "$VERSION" ] && VERSION=$(default_version)
[ -z "$TARGET" ]  && TARGET="2.0alpha"
[ -z "$AUTHOR" ]  && AUTHOR="`git config user.name` <`git config \
                                  user.email`>"


kind=0
[ -n "$OBS" ] && kind=$(($kind + 1))
[ -n "$GBS" ] && kind=$(($kind + 1))

if [ "$kind" -gt 1 ]; then
    error "Multiple release types specified (--obs, --gbs)."
    exit 1
else
    if [ "$kind" = "0" ]; then
        GBS=2.0alpha
    fi
fi

echo "Release configuration for package $PKG:"
echo "    releasing:  $RELEASE"
echo "    as version: $VERSION"
echo "      baseline: ${BASELINE:--}"

# enable recording and excuting undo actions
enable_auto_undo

if [ -n "$OBS" ]; then
    obs_export
else
    gbs_prepare
fi
