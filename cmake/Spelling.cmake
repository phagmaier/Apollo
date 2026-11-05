# Unmodified Hunspell 1.7.2 sources; retain upstream license notices in the
# fetched archive. No system installation or configure-time executable runs.
include(FetchContent)
FetchContent_Declare(hunspell
  URL https://codeload.github.com/hunspell/hunspell/tar.gz/refs/tags/v1.7.2
  URL_HASH SHA256=69fa312d3586c988789266eaf7ffc9861d9f6396c31fc930a014d551b59bbd6e)
FetchContent_MakeAvailable(hunspell)
set(hunspell_src "${hunspell_SOURCE_DIR}/src/hunspell")
set(HAVE_VISIBILITY 1)
configure_file("${hunspell_src}/hunvisapi.h.in" "${hunspell_BINARY_DIR}/hunvisapi.h" @ONLY)
add_library(apollo_hunspell STATIC
  "${hunspell_src}/affentry.cxx" "${hunspell_src}/affixmgr.cxx"
  "${hunspell_src}/csutil.cxx" "${hunspell_src}/filemgr.cxx"
  "${hunspell_src}/hashmgr.cxx" "${hunspell_src}/hunspell.cxx"
  "${hunspell_src}/hunzip.cxx" "${hunspell_src}/phonet.cxx"
  "${hunspell_src}/replist.cxx" "${hunspell_src}/suggestmgr.cxx")
target_include_directories(apollo_hunspell PUBLIC "${hunspell_src}" "${hunspell_BINARY_DIR}")
target_compile_definitions(apollo_hunspell PUBLIC HUNSPELL_STATIC)
# Dictionary data is committed so running the editor needs no download.
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/assets/spelling/en_US.aff" spelling_aff_hash)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/assets/spelling/en_US.dic" spelling_dic_hash)
if(NOT spelling_aff_hash STREQUAL "c7a8c4d08c29d237880844b1623099f59092602f189be38ce3912e457ff38bc1"
   OR NOT spelling_dic_hash STREQUAL "f0b1a234bd178bdd01875b2a392a9647f888b8fe879f79c52aae62c2759b3647")
  message(FATAL_ERROR "Pinned US English dictionary hash mismatch")
endif()
