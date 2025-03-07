{
  inputs,
  versionShortOverride,
  versionLongOverride,
}: final: prev: let
  inherit (final) lib;
  inherit (final.stdenv.hostPlatform) isStatic;
  stdenv =
    if final.stdenv.isDarwin
    then final.llvmPackages_16.stdenv
    else final.stdenv;
in {
  google-cloud-cpp =
    if !isStatic
    then prev.google-cloud-cpp
    else
    prev.google-cloud-cpp.overrideAttrs (orig: {
        buildInputs = orig.buildInputs ++ [ final.gbenchmark ];
        propagatedNativeBuildInputs = (orig.propagatedNativeBuildInputs or []) ++ [prev.buildPackages.pkg-config];
        patches = (orig.patches or []) ++ [
          ./google-cloud-cpp/0001-Use-pkg-config-to-find-CURL.patch
        ];
      });
  aws-c-cal =
    if !isStatic
    then prev.aws-c-cal
    else
      prev.aws-c-cal.overrideAttrs (orig: {
        patches = (orig.patches or []) ++ [
          (prev.fetchpatch {
            url = "https://github.com/awslabs/aws-c-cal/commit/ee46efc3dd0cf300ff4ec89cc2d79f1b0fe1c8cb.patch";
            sha256 = "sha256-bFc0Mqt0Ho3i3xGHiQitP35dQgPd9Wthkyb1TT/nRYs=";
          })
        ];
      });
  arrow-cpp =
    if !isStatic
    then prev.arrow-cpp
    else
      (prev.arrow-cpp.override {
        enableShared = false;
        google-cloud-cpp = final.google-cloud-cpp.override {
          apis = [ "storage" ];
        };
      })
      .overrideAttrs (orig: {
        buildInputs = orig.buildInputs ++ [final.sqlite];
        cmakeFlags =
          orig.cmakeFlags
          ++ [
            # Needed for correct dependency resolution, should be the default...
            "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON"
            # Backtrace doesn't build in static mode, need to investigate.
            "-DARROW_WITH_BACKTRACE=OFF"
            "-DARROW_BUILD_TESTS=OFF"
          ];
        doCheck = false;
        doInstallCheck = false;
      });
  zeromq =
    if !isStatic
    then prev.zeromq
    else
    prev.zeromq.overrideAttrs (orig: {
      cmakeFlags = orig.cmakeFlags ++ [
        "-DBUILD_SHARED=OFF"
        "-DBUILD_STATIC=ON"
        "-DBUILD_TESTS=OFF"
      ];
    });
  grpc =
    if !isStatic
    then prev.grpc
    else
      prev.grpc.overrideAttrs (orig: {
        patches =
          orig.patches
          ++ [
            ./grpc/drop-broken-cross-check.patch
          ];
      });
  http-parser =
    if !isStatic
    then prev.http-parser
    else
      prev.http-parser.overrideAttrs (_: {
        postPatch = let
          cMakeLists = prev.writeTextFile {
            name = "http-parser-cmake";
            text = ''
              cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
              project(http_parser)
              include(GNUInstallDirs)
              add_library(http_parser http_parser.c)
              target_compile_options(http_parser PRIVATE -Wall -Wextra)
              target_include_directories(http_parser PUBLIC .)
              set_target_properties(http_parser PROPERTIES PUBLIC_HEADER http_parser.h)
              install(
                TARGETS http_parser
                ARCHIVE DESTINATION "''${CMAKE_INSTALL_LIBDIR}"
                LIBRARY DESTINATION "''${CMAKE_INSTALL_LIBDIR}"
                RUNTIME DESTINATION "''${CMAKE_INSTALL_BINDIR}"
                PUBLIC_HEADER DESTINATION "''${CMAKE_INSTALL_INCLUDEDIR}")
            '';
          };
        in ''
          cp ${cMakeLists} CMakeLists.txt
        '';
        nativeBuildInputs = [prev.buildPackages.cmake];
        makeFlags = [];
        buildFlags = [];
        doCheck = false;
      });
  libbacktrace =
    if !isStatic
    then prev.libbacktrace
    else
      prev.libbacktrace.overrideAttrs (old: {
        doCheck = false;
      });
  rdkafka = prev.rdkafka.overrideAttrs (orig: {
    nativeBuildInputs = orig.nativeBuildInputs ++ [prev.buildPackages.cmake];
    # The cmake config file doesn't find them if they are not propagated.
    propagatedBuildInputs = orig.buildInputs;
    cmakeFlags = lib.optionals isStatic [
      "-DRDKAFKA_BUILD_STATIC=ON"
      # The interceptor tests library is hard-coded to SHARED.
      "-DRDKAFKA_BUILD_TESTS=OFF"
    ];
  });
  mkStub = name: prev.writeShellScriptBin name ''
    echo "stub-${name}: $@" >&2
  '';
  fluent-bit =
    if !isStatic
    then prev.fluent-bit
    else prev.fluent-bit.overrideAttrs (orig: {
      outputs = ["out"];
      nativeBuildInputs = orig.nativeBuildInputs ++ [(final.mkStub "ldconfig")];
      # Neither systemd nor postgresql have a working static build.
      buildInputs = [ final.musl-fts final.openssl final.libyaml ];
      propagatedBuildInputs = [ final.musl-fts final.openssl final.libyaml ];
      cmakeFlags = [
        "-DFLB_RELEASE=ON"
        "-DFLB_BINARY=OFF"
        "-DFLB_SHARED_LIB=OFF"
        "-DFLB_METRICS=ON"
        "-DFLB_HTTP_SERVER=ON"
        "-DFLB_LUAJIT=OFF"
      ];
      # The build scaffold of fluent-bit doesn't install static libraries, so we
      # work around it by just copying them from the build directory. The
      # blacklist is hand-written and prevents the inclusion of duplicates in
      # the linker command line when building the fluent-bit plugin.
      # The Findfluent-bit.cmake module then globs all archives into list for
      # `target_link_libraries` to get a working link.
      postInstall = let
        archive-blacklist = [
          "libbacktrace.a"
          "librdkafka.a"
          "libxxhash.a"
        ];

      in ''
        set -x
        mkdir -p $out/lib
        find . -type f \( -name "*.a" ${lib.concatMapStrings (x: " ! -name \"${x}\"") archive-blacklist} \) \
               -exec cp "{}" $out/lib/ \;
        set +x
      '';
    });
  restinio = final.callPackage ./restinio {};
  caf = let
    source = builtins.fromJSON (builtins.readFile ./caf/source.json);
  in
    (prev.caf.override {inherit stdenv;}).overrideAttrs (old:
      {
        # fetchFromGitHub uses ellipsis in the parameter set to be hash method
        # agnostic. Because of that, callPackageWith does not detect that sha256
        # is a required argument, and it has to be passed explicitly instead.
        src = prev.fetchFromGitHub {inherit (source) owner repo rev sha256;};
        inherit (source) version;
        # The OpenSSL dependency appears in the interface of CAF, so it has to
        # be propagated downstream.
        propagatedBuildInputs = [final.openssl];
        NIX_CFLAGS_COMPILE = "-fno-omit-frame-pointer";
        # Building statically implies using -flto. Since we produce a final binary with
        # link time optimizaitons in Tenzir, we need to make sure that type definitions that
        # are parsed in both projects are the same, otherwise the compiler will complain
        # at the optimization stage.
        # https://github.com/NixOS/nixpkgs/issues/130963
        NIX_LDFLAGS = lib.optionalString stdenv.isDarwin "-lc++abi";
        preCheck = ''
          export LD_LIBRARY_PATH=$PWD/lib
          export DYLD_LIBRARY_PATH=$PWD/lib
        '';
      }
      // lib.optionalAttrs isStatic {
        cmakeFlags =
          old.cmakeFlags
          ++ [
            "-DCAF_BUILD_STATIC=ON"
            "-DCAF_BUILD_STATIC_ONLY=ON"
            "-DCAF_ENABLE_TESTING=OFF"
            "-DOPENSSL_USE_STATIC_LIBS=TRUE"
            "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION:BOOL=ON"
            "-DCMAKE_POLICY_DEFAULT_CMP0069=NEW"
          ];
        hardeningDisable = [
          "fortify"
          "pic"
        ];
        dontStrip = true;
        doCheck = false;
      });
  fast_float = final.callPackage ./fast_float {};
  jemalloc =
    if !isStatic
    then prev.jemalloc
    else
      prev.jemalloc.overrideAttrs (old: {
        EXTRA_CFLAGS = (old.EXTRA_CFLAGS or "") + " -fno-omit-frame-pointer";
        configureFlags = old.configureFlags ++ ["--enable-prof" "--enable-stats"];
        doCheck = !isStatic;
      });
  tenzir-source = inputs.nix-filter.lib.filter {
    root = ./..;
    include = [
      (inputs.nix-filter.lib.inDirectory ../changelog)
      (inputs.nix-filter.lib.inDirectory ../cmake)
      (inputs.nix-filter.lib.inDirectory ../contrib)
      (inputs.nix-filter.lib.inDirectory ../docs)
      (inputs.nix-filter.lib.inDirectory ../docs)
      (inputs.nix-filter.lib.inDirectory ../libtenzir)
      (inputs.nix-filter.lib.inDirectory ../libtenzir_test)
      (inputs.nix-filter.lib.inDirectory ../plugins)
      (inputs.nix-filter.lib.inDirectory ../python)
      (inputs.nix-filter.lib.inDirectory ../schema)
      (inputs.nix-filter.lib.inDirectory ../scripts)
      (inputs.nix-filter.lib.inDirectory ../tenzir)
      ../VERSIONING.md
      ../CMakeLists.txt
      ../LICENSE
      ../README.md
      ../tenzir.spdx.json
      ../VERSIONING.md
      ../tenzir.yaml.example
      ../version.json
    ];
  };
  tenzir-de = final.callPackage ./tenzir {
    inherit stdenv versionShortOverride versionLongOverride;
    pname = "tenzir-de";
  };
  # Policy: The suffix-less `tenzir' packages come with a few closed source
  # plugins.
  tenzir = let
    pkg = final.tenzir-de.override {
      pname = "tenzir";
    };
  in
    pkg.withPlugins (ps: [
      ps.matcher
      ps.netflow
      ps.pipeline_manager
      ps.platform
    ]);
  tenzir-cm = let
    pkg = final.tenzir-de.override {
      pname = "tenzir-cm";
    };
  in
    pkg.withPlugins (ps: [
      ps.compaction
      ps.matcher
    ]);
  tenzir-ee = let
    pkg = final.tenzir-de.override {
      pname = "tenzir-ee";
    };
  in
    pkg.withPlugins (ps: [
      ps.compaction
      #ps.inventory
      ps.matcher
      ps.netflow
      ps.pipeline_manager
      ps.platform
    ]);
  tenzir-functional-test-deps = let
    bats = prev.bats.withLibraries (p: [
      p.bats-support
      p.bats-assert
    ]);
  in [ bats prev.curl prev.jq ];
  tenzir-integration-test-deps = let
    py3 = prev.python3.withPackages (ps:
      with ps; [
        coloredlogs
        jsondiff
        pyarrow
        pyyaml
        schema
      ]);
  in [py3 final.jq final.tcpdump];
  speeve = final.buildGoModule rec {
    pname = "speeve";
    version = "0.1.3";
    vendorSha256 = "sha256-Mw1cRIwmDS2Canljkuw96q2+e+z14MUcU5EtupUcTDQ=";
    src = final.fetchFromGitHub {
      rev = "v${version}";
      owner = "satta";
      repo = pname;
      hash = "sha256-75QrtuOduUNT9g2RJRWUow8ESBqsDDXCMGVNQKFc+SE=";
    };
    # upstream does not provide a go.sum file
    preBuild = ''
      cp ${./speeve-go.sum} go.sum
    '';
    subPackages = ["cmd/speeve"];
  };
}
