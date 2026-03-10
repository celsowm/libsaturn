# MVP Notes

## Escopo implementado

- Video: NTSC 320x224.
- Endianness fixa: SH2 big-endian (`-mb`).
- API pública sem `float`.
- Sem alocação dinâmica implícita no core runtime.

## Fora do MVP

- Pipeline 3D completo.
- SCU DSP avançado.
- Driver completo M68k/SCSP.
- Multiprocessamento Master/Slave SH2 para gameplay.

## Critérios de aceite para execução manual

1. Build completo sem intervenção manual após setup.
2. ISO inicia em emulador e entra no loop principal.
3. Render de sprites 2D estável por 1800 frames.
4. Input sem ghost presses por 5 minutos.
5. Conversor de assets coberto por teste automatizado.

