"""Pré-gera assets convertidos para evitar dependência do Pillow no build."""
import argparse
import json
import sys
from pathlib import Path

# Adicionar tools ao path para importar convert_indexed8
sys.path.insert(0, str(Path(__file__).parent))
from convert_indexed8 import convert_asset


def bake_asset(input_path: Path, output_prefix: str, resize: tuple[int, int] | None) -> None:
    """Converte uma imagem e salva os arquivos .h/.c no diretório prebuilt."""
    from convert_indexed8 import convert_asset

    out_prefix = Path(output_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)

    result = convert_asset(
        in_path=input_path,
        out_prefix=out_prefix,
        palette_path=None,
        width_arg=None,
        height_arg=None,
        palette_index=0,
        resize_to=resize,
    )
    print(f"  [bake] {input_path.name} -> {out_prefix.parent.name}/{out_prefix.stem} "
          f"({result.get('pixels', {}).get('size', 'N/A')} pixels)")


def main():
    parser = argparse.ArgumentParser(description="Bake assets para build rápido")
    parser.add_argument(
        "assets_json",
        nargs="?",
        default="assets/prebuilt/manifest.json",
        help="Manifest JSON com lista de assets (default: assets/prebuilt/manifest.json)",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Listar assets disponíveis sem converter",
    )
    args = parser.parse_args()

    manifest_path = Path(args.assets_json)
    if not manifest_path.exists():
        print(f"[bake] Manifest não encontrado: {manifest_path}")
        print("[bake] Rode: python tools/bake-assets.py --init para criar um template")
        sys.exit(1)

    manifest = json.loads(manifest_path.read_text())
    assets_dir = manifest_path.parent

    if args.list:
        print("Assets pré-convertidos disponíveis:")
        for name, info in manifest.get("assets", {}).items():
            resize = info.get("resize", "")
            print(f"  {name} -> {info['output']} (resize: {resize})")
        return

    print(f"[bake] Pré-gerando {len(manifest.get('assets', {}))} assets...")
    for name, info in manifest["assets"].items():
        input_path = Path(info["input"])
        if not input_path.is_absolute():
            input_path = assets_dir / input_path

        if not input_path.exists():
            print(f"  [skip] {input_path} não encontrado")
            continue

        output_prefix = str(assets_dir / info["output"])
        resize = tuple(info["resize"]) if info.get("resize") else None
        bake_asset(input_path, output_prefix, resize)

    print("[bake] Concluído!")


if __name__ == "__main__":
    main()
