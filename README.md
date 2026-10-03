# PhysicsCursor

Simulação em C++ e plugin do Hyprland para inclinar o cursor com mola,
amortecimento e inércia. O daemon calcula a física a 500 Hz e envia o ângulo
por memória compartilhada. A ponta de clique continua na posição do mouse.

## Instalar no CachyOS / Arch Linux

```bash
git clone https://github.com/Y-Workplace/PhysicsCursor.git
cd PhysicsCursor
bash install.sh
```

O instalador busca as dependências ausentes com `pacman` (pode pedir a senha
do `sudo`), compila, executa os testes e instala os binários. Você não precisa
baixar bibliotecas ou procurar cabeçalhos manualmente. A primeira instalação
precisa de internet se faltar algum pacote.

O repositório inclui os fontes do simulador e do plugin, scripts, parâmetros
compiláveis e testes. As bibliotecas do sistema são instaladas pelo gerenciador
de pacotes. O plugin é recompilado contra os cabeçalhos do Hyprland instalado,
pois sua ABI depende da versão do compositor. Após atualizar o Hyprland,
execute novamente `bash install.sh`.

Os executáveis são instalados em `~/.local/bin/physics_cursor` e
`~/.local/share/hyprland/plugins/dynamic-cursors.so`. O instalador configura
`hyprland.lua` ou `hyprland.conf` e inicia o daemon. O instalador recarrega o plugin na sessão acessível do Hyprland. Se ele não
conseguir acessar o compositor, reinicie sua sessão para carregar o novo binário.

## Escolher parâmetros, compilar e testar na simulação

```bash
bash run.sh
# Ou, em overlay transparente:
bash run.sh --overlay
```

Ajuste os parâmetros com as teclas abaixo e pressione **F9**:

1. Os valores atuais são gravados em `src/PhysicsDefaults.hpp`.
2. O simulador e o daemon são recompilados com esses valores.
3. Os testes verificam estabilidade durante sacudidas rápidas, limite angular,
   retorno ao repouso, cadência e exportação dos parâmetros.
4. Se tudo passar, o daemon é reiniciado com a configuração compilada.
   Se houver uma instalação em `~/.local/bin`, ela também é atualizada.

A janela continua aberta durante a compilação e mostra o resultado no HUD.
O log fica em `build/playground-build.log`. Execute a simulação a partir do
checkout do projeto para editar e compilar seus fontes. Instale primeiro com
`bash install.sh` para preparar o plugin e as ferramentas de compilação.
O daemon anterior é preservado se a compilação ou os testes falharem.

| Tecla | Função |
|---|---|
| 1 / 2 | Diminuir / aumentar arrasto |
| 3 / 4 | Diminuir / aumentar rigidez da mola |
| 5 / 6 | Diminuir / aumentar amortecimento |
| 7 / 8 | Diminuir / aumentar influência inercial |
| F9 | Salvar valores, compilar, testar e aplicar ao daemon |
| R | Restaurar os valores compilados desta janela |
| C | Alternar cursor do sistema / vetorial |
| L | Alternar português / inglês |
| V / T / P | Vetores / rastro / pivô |
| + / - | Aumentar / diminuir cursor na simulação |
| Espaço | Aplicar impulso angular |
| H | Mostrar / ocultar HUD |
| F11 / O | Alternar janela / overlay |
| Esc / Q | Fechar a janela, preservando o daemon |

Valores iniciais: massa `1`, mola `155`, amortecimento `4`, arrasto `0.00360`,
inércia `0.00005`, deflexão máxima `45°`. O mesmo arquivo de parâmetros é
usado pelo simulador e pelo daemon.

## Cursor ampliado ao sacudir

Sacudir rapidamente aumenta o cursor e mantém sua rotação física. O plugin
preserva a física em modo `tilt`, mesmo com `shake:effects = false` em uma
configuração antiga, e preserva o pivô da textura ampliada,
inclusive ao usar uma imagem de maior resolução do tema. O cursor ampliado
usa renderização por software, com uma região de desenho que inclui sua rotação.

Se sua configuração antiga desabilitar os efeitos, habilite-os:

```ini
plugin:dynamic-cursors {
    mode = tilt
    threshold = 0
    shake:enabled = true
    shake:effects = true
}
```

Em Lua, no bloco `plugin.dynamic_cursors`:

```lua
shake = { enabled = true, effects = true },
```

## Compilar sem instalar

```bash
bash build.sh               # Dependências, simulador, testes e plugin
bash build.sh --daemon-only # Apenas simulador / daemon e testes
make test                  # Testes sem SDL ou sessão gráfica
```

Use `JOBS=4 bash build.sh` para escolher a quantidade de compilações paralelas
(o padrão é 2). O caminho com CMake também inclui os testes:

```bash
bash scripts/bootstrap.sh --daemon-only
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

## Controlar o daemon

```bash
bash start_daemon.sh
bash stop_daemon.sh
pgrep -fa 'physics_cursor --daemon'
```

Ao encerrar o daemon, o plugin usa sua física local como alternativa em modo
`tilt`. Abrir e fechar a simulação não encerra o daemon. As alterações visuais
não modificam os eventos de clique ou o movimento enviado aos aplicativos.

Os testes são executados pelo carregador ELF, assim como o simulador. Isso
permite executar o fluxo a partir de volumes como `/mnt/archives` que não
preservam a permissão de execução dos binários compilados.
