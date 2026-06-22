{
  description = "Go dev shell for M-STEP CLI port";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in {
        devShells.default = pkgs.mkShell {
          name = "cli-go-shell";

            buildInputs = [
                pkgs.go
            ];

            shellHook = ''
                echo "ARM dev environment ready."
            
                # If fish exists, open it
                if command -v fish &> /dev/null; then
                    exec fish
                fi
            '';
        };
      }
    );
}
