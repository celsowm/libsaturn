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
