# AGENTS

## Execution Policy (Mandatory)

- O agente deve **sempre executar** comandos de build, run e emulador por conta própria.
- O agente deve usar os scripts do projeto (`build-example.ps1`, `run-example.ps1`, launchers em `emulators/`) como caminho padrão.
- O agente **não deve** pedir para o usuário rodar manualmente, exceto em bloqueio real fora do controle do agente (ex.: janela do SO não visível, permissão externa, crash do emulador).
- Quando houver bloqueio real, o agente deve:
  - descrever objetivamente o bloqueio;
  - tentar uma alternativa automática;
  - só então pedir uma ação mínima do usuário.

## Practical Expectation

- Sempre entregar status claro: comando executado, resultado e próximo passo.

## Hardware / Audio Context

- Antes de alterar SCSP, streaming de música ou testes relacionados, leia `docs/SCSP_AUDIO_STREAMING_GUIDE.md`.
- Trate os manuais em `docs/sega_saturn_hardware/` como fonte primária para semântica de registradores.
- Para regressões de áudio, prefira invariantes observáveis (registradores SCSP, posição da amostra, Sound RAM e contadores de underrun/refill) a ajustes empíricos de volume/pitch.

