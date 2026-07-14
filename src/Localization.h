#pragma once
#include <string>
#include <unordered_map>

enum class Language { EN = 0, PT_BR = 1, ES = 2, ZH = 3 };

class Localization {
public:
    static void SetLanguage(Language lang) { current = lang; }
    static void SetLanguage(int langIdx) { current = static_cast<Language>(langIdx); }
    static Language GetLanguage() { return current; }
    static int GetLanguageIndex() { return static_cast<int>(current); }

    static const char* T(const std::string& key) {
        auto it = strings.find(key);
        if (it == strings.end()) return key.c_str();
        auto langIt = it->second.find(current);
        if (langIt == it->second.end()) return it->second.at(Language::EN).c_str();
        return langIt->second.c_str();
    }

private:
    static inline Language current = Language::PT_BR;

    static inline std::unordered_map<std::string, std::unordered_map<Language, std::string>> strings = {
        {"window_title", {
            {Language::EN, "DuckFaceLLM"}, {Language::PT_BR, "DuckFaceLLM"},
            {Language::ES, "DuckFaceLLM"}, {Language::ZH, "DuckFaceLLM"}
        }},
        {"tab_params", {
            {Language::EN, "Parameters"}, {Language::PT_BR, "Parâmetros"},
            {Language::ES, "Parámetros"}, {Language::ZH, "参数"}
        }},
        {"sampling_params_title", {
            {Language::EN, "Sampling parameters"}, {Language::PT_BR, "Parâmetros de amostragem"},
            {Language::ES, "Parámetros de muestreo"}, {Language::ZH, "采样参数"}
        }},
        {"param_temperature", {
            {Language::EN, "Temperature"}, {Language::PT_BR, "Temperatura"},
            {Language::ES, "Temperatura"}, {Language::ZH, "温度"}
        }},
        {"param_top_k", {
            {Language::EN, "Top-K"}, {Language::PT_BR, "Top-K"},
            {Language::ES, "Top-K"}, {Language::ZH, "Top-K"}
        }},
        {"param_top_p", {
            {Language::EN, "Top-P"}, {Language::PT_BR, "Top-P"},
            {Language::ES, "Top-P"}, {Language::ZH, "Top-P"}
        }},
        {"param_min_p", {
            {Language::EN, "Min-P"}, {Language::PT_BR, "Min-P"},
            {Language::ES, "Min-P"}, {Language::ZH, "Min-P"}
        }},
        {"param_repeat_penalty", {
            {Language::EN, "Repeat penalty"}, {Language::PT_BR, "Penalidade de repetição"},
            {Language::ES, "Penalización de repetición"}, {Language::ZH, "重复惩罚"}
        }},
        {"param_max_tokens", {
            {Language::EN, "Max tokens"}, {Language::PT_BR, "Máx. de tokens"},
            {Language::ES, "Máx. de tokens"}, {Language::ZH, "最大令牌数"}
        }},
        {"param_ctx_size", {
            {Language::EN, "Context size"}, {Language::PT_BR, "Tamanho do contexto"},
            {Language::ES, "Tamaño de contexto"}, {Language::ZH, "上下文大小"}
        }},
        {"param_reset", {
            {Language::EN, "Reset to defaults"}, {Language::PT_BR, "Restaurar padrão"},
            {Language::ES, "Restaurar valores"}, {Language::ZH, "恢复默认值"}
        }},
        {"param_hint", {
            {Language::EN, "Lower temperature = more focused/deterministic. Higher = more creative but can ramble. If the model rambles a lot, try temperature 0.3-0.6 and repeat penalty 1.1-1.3."},
            {Language::PT_BR, "Temperatura mais baixa = mais focado/determinístico. Mais alta = mais criativo, mas pode delirar. Se o modelo estiver delirando muito, tente temperatura 0.3-0.6 e penalidade de repetição 1.1-1.3."},
            {Language::ES, "Temperatura más baja = más enfocado/determinista. Más alta = más creativo, pero puede divagar. Si el modelo divaga mucho, prueba temperatura 0.3-0.6 y penalización 1.1-1.3."},
            {Language::ZH, "温度越低=越专注确定；越高=越有创意但可能胡言乱语。如果模型胡言乱语严重，试试温度 0.3-0.6 和重复惩罚 1.1-1.3。"}
        }},
        {"system_prompt_label", {
            {Language::EN, "System prompt (base instructions sent with every message)"},
            {Language::PT_BR, "Prompt de sistema (instruções base enviadas em toda mensagem)"},
            {Language::ES, "Prompt de sistema (instrucciones base enviadas en cada mensaje)"},
            {Language::ZH, "系统提示词（随每条消息发送的基础指令）"}
        }},
        {"ctx_restart_hint", {
            {Language::EN, "Restart the server for this to take effect."},
            {Language::PT_BR, "Reinicie o servidor para isso ter efeito."},
            {Language::ES, "Reinicia el servidor para que esto tenga efecto."},
            {Language::ZH, "重启服务器后生效。"}
        }},
        {"gguf_only_note", {
            {Language::EN, "DuckFaceLLM's engine (llama.cpp) only runs .gguf files, so only .gguf models can be downloaded here."},
            {Language::PT_BR, "O motor do DuckFaceLLM (llama.cpp) só roda arquivos .gguf, então aqui só é possível baixar modelos .gguf."},
            {Language::ES, "El motor de DuckFaceLLM (llama.cpp) solo ejecuta archivos .gguf, así que aquí solo se pueden descargar modelos .gguf."},
            {Language::ZH, "DuckFaceLLM 的引擎（llama.cpp）只能运行 .gguf 文件，因此这里只能下载 .gguf 格式的模型。"}
        }},
        {"tab_templates", {
            {Language::EN, "Prompt Templates"}, {Language::PT_BR, "Templates de Prompt"},
            {Language::ES, "Plantillas de Prompt"}, {Language::ZH, "提示词模板"}
        }},
        {"auto_template_label", {
            {Language::EN, "Auto-select best prompt template"}, {Language::PT_BR, "Selecionar automaticamente o melhor template"},
            {Language::ES, "Seleccionar automáticamente la mejor plantilla"}, {Language::ZH, "自动选择最佳模板"}
        }},
        {"template_override_label", {
            {Language::EN, "Force template"}, {Language::PT_BR, "Forçar template"},
            {Language::ES, "Forzar plantilla"}, {Language::ZH, "强制使用模板"}
        }},
        {"template_none", {
            {Language::EN, "(none / generic)"}, {Language::PT_BR, "(nenhum / genérico)"},
            {Language::ES, "(ninguna / genérica)"}, {Language::ZH, "（无/通用）"}
        }},
        {"template_active_label", {
            {Language::EN, "Template in use:"}, {Language::PT_BR, "Template em uso:"},
            {Language::ES, "Plantilla en uso:"}, {Language::ZH, "使用中的模板："}
        }},
        {"templates_list_title", {
            {Language::EN, "Built-in prompt templates"}, {Language::PT_BR, "Templates de prompt inclusos"},
            {Language::ES, "Plantillas de prompt incluidas"}, {Language::ZH, "内置提示词模板"}
        }},
        {"templates_hint", {
            {Language::EN, "These are specialized system prompts DuckFaceLLM automatically picks from based on what you ask — similar to how this very assistant selects the right approach for coding, debugging, writing, etc. You can also force one manually in the Chat tab, and add your own below."},
            {Language::PT_BR, "Estes são prompts de sistema especializados que o DuckFaceLLM escolhe automaticamente com base no que você pede — parecido com como este próprio assistente seleciona a abordagem certa para código, debug, escrita, etc. Você também pode forçar um manualmente na aba Conversa, e adicionar os seus próprios abaixo."},
            {Language::ES, "Estos son prompts de sistema especializados que DuckFaceLLM elige automáticamente según lo que pides, similar a cómo este propio asistente selecciona el enfoque correcto para código, depuración, escritura, etc. También puedes forzar uno manualmente en la pestaña Chat, y agregar los tuyos abajo."},
            {Language::ZH, "这些是 DuckFaceLLM 根据您的请求自动选择的专用系统提示词——类似于本助手为编码、调试、写作等选择合适方法的方式。您也可以在对话标签中手动强制选择，并在下方添加您自己的模板。"}
        }},
        {"mmproj_label", {
            {Language::EN, "Multimodal projector (mmproj) file — optional, required for vision models"},
            {Language::PT_BR, "Arquivo do projetor multimodal (mmproj) — opcional, necessário para modelos com visão"},
            {Language::ES, "Archivo del proyector multimodal (mmproj) — opcional, necesario para modelos con visión"},
            {Language::ZH, "多模态投影文件（mmproj）——可选，视觉模型需要"}
        }},
        {"mmproj_hint", {
            {Language::EN, "Only needed if your GGUF model supports images (e.g. LLaVA, Qwen2-VL) and you want to attach images."},
            {Language::PT_BR, "Só é necessário se seu modelo GGUF suportar imagens (ex: LLaVA, Qwen2-VL) e você quiser anexar imagens."},
            {Language::ES, "Solo es necesario si tu modelo GGUF admite imágenes (ej: LLaVA, Qwen2-VL) y quieres adjuntar imágenes."},
            {Language::ZH, "仅当您的 GGUF 模型支持图像（如 LLaVA、Qwen2-VL）且您想附加图像时才需要。"}
        }},
        {"attach_button", {
            {Language::EN, "Attach"}, {Language::PT_BR, "Anexar"},
            {Language::ES, "Adjuntar"}, {Language::ZH, "附加"}
        }},
        {"remove_attachment", {
            {Language::EN, "Remove"}, {Language::PT_BR, "Remover"},
            {Language::ES, "Quitar"}, {Language::ZH, "移除"}
        }},
        {"browse", {
            {Language::EN, "Browse..."}, {Language::PT_BR, "Procurar..."},
            {Language::ES, "Examinar..."}, {Language::ZH, "浏览..."}
        }},
        {"use_in_chat", {
            {Language::EN, "Use in Chat"}, {Language::PT_BR, "Usar na Conversa"},
            {Language::ES, "Usar en Chat"}, {Language::ZH, "在对话中使用"}
        }},
        {"use_in_agent", {
            {Language::EN, "Use in Agent"}, {Language::PT_BR, "Usar no Agente"},
            {Language::ES, "Usar en Agente"}, {Language::ZH, "在代理中使用"}
        }},
        {"tab_projects", {
            {Language::EN, "Projects"}, {Language::PT_BR, "Projetos"},
            {Language::ES, "Proyectos"}, {Language::ZH, "项目"}
        }},
        {"new_project", {
            {Language::EN, "New Project"}, {Language::PT_BR, "Novo Projeto"},
            {Language::ES, "Nuevo Proyecto"}, {Language::ZH, "新项目"}
        }},
        {"no_projects_yet", {
            {Language::EN, "No saved agent projects yet. Start a task in the Agent tab and it will show up here."},
            {Language::PT_BR, "Nenhum projeto do agente salvo ainda. Inicie uma tarefa na aba Agente e ela aparece aqui."},
            {Language::ES, "Aún no hay proyectos del agente guardados. Inicia una tarea en la pestaña Agente y aparecerá aquí."},
            {Language::ZH, "还没有保存的代理项目。在代理标签中开始一个任务，它会显示在这里。"}
        }},
        {"delete_project_confirm", {
            {Language::EN, "Delete this project's conversation? This cannot be undone (the files it created stay on disk)."},
            {Language::PT_BR, "Excluir a conversa deste projeto? Isso não pode ser desfeito (os arquivos criados continuam no disco)."},
            {Language::ES, "¿Eliminar la conversación de este proyecto? Esto no se puede deshacer (los archivos creados permanecen en el disco)."},
            {Language::ZH, "删除此项目的对话？此操作无法撤销（已创建的文件仍保留在磁盘上）。"}
        }},
        {"large_context_hint", {
            {Language::EN, "Sliders go up to 1,048,576 (~1M) and use a logarithmic scale for easier small-value control; type an exact number in the box next to each slider if you need a precise value. Very large context sizes only work if the model itself supports them and your RAM/VRAM can hold it - check the model's card on Hugging Face for its actual maximum context length."},
            {Language::PT_BR, "Os sliders vão até 1.048.576 (~1M) e usam escala logarítmica para facilitar ajustar valores pequenos; digite um número exato na caixa ao lado de cada slider se precisar de um valor preciso. Contextos muito grandes só funcionam se o próprio modelo suportar e sua RAM/VRAM aguentar - confira o card do modelo no Hugging Face para saber o tamanho máximo de contexto real dele."},
            {Language::ES, "Los deslizadores llegan hasta 1,048,576 (~1M) y usan escala logarítmica para facilitar ajustar valores pequeños; escribe un número exacto en la caja junto a cada deslizador si necesitas un valor preciso. Contextos muy grandes solo funcionan si el modelo lo soporta y tu RAM/VRAM lo permite - revisa la ficha del modelo en Hugging Face para su longitud de contexto máxima real."},
            {Language::ZH, "滑块最大可到 1,048,576（约100万），并使用对数刻度以便更容易调整较小的值；如需精确数值，可在滑块旁的输入框中直接输入。非常大的上下文只有在模型本身支持、且您的内存/显存足够时才有效——请查看 Hugging Face 上该模型页面的实际最大上下文长度。"}
        }},
        {"agent_action_delete", {
            {Language::EN, "Delete file:"}, {Language::PT_BR, "Apagar arquivo:"},
            {Language::ES, "Eliminar archivo:"}, {Language::ZH, "删除文件："}
        }},
        {"agent_delete_warning", {
            {Language::EN, "This file will be permanently deleted. This cannot be undone."},
            {Language::PT_BR, "Este arquivo será apagado permanentemente. Isso não pode ser desfeito."},
            {Language::ES, "Este archivo será eliminado permanentemente. Esto no se puede deshacer."},
            {Language::ZH, "此文件将被永久删除。此操作无法撤销。"}
        }},
        {"reviewer_section_title", {
            {Language::EN, "Reviewer Model (optional)"}, {Language::PT_BR, "Modelo Revisor (opcional)"},
            {Language::ES, "Modelo Revisor (opcional)"}, {Language::ZH, "审核模型（可选）"}
        }},
        {"reviewer_section_hint", {
            {Language::EN, "A second, independent model that checks each file the Creator writes and gives it specific feedback to fix - instead of the Creator only reviewing its own work. Runs on its own server, so it can be a different model. Needs more RAM/VRAM (two models loaded at once) and roughly doubles the time per step."},
            {Language::PT_BR, "Um segundo modelo, independente, que confere cada arquivo escrito pelo Criador e dá feedback específico para corrigir - em vez do Criador só revisar o próprio trabalho. Roda no seu próprio servidor, então pode ser um modelo diferente. Precisa de mais RAM/VRAM (dois modelos carregados ao mesmo tempo) e praticamente dobra o tempo por passo."},
            {Language::ES, "Un segundo modelo, independiente, que revisa cada archivo escrito por el Creador y da retroalimentación específica para corregir - en vez de que el Creador solo revise su propio trabajo. Se ejecuta en su propio servidor, así que puede ser un modelo diferente. Necesita más RAM/VRAM (dos modelos cargados a la vez) y prácticamente duplica el tiempo por paso."},
            {Language::ZH, "一个独立的第二模型，检查创建者写的每个文件并给出具体的修正意见——而不是让创建者只审核自己的工作。运行在自己的服务器上，因此可以是不同的模型。需要更多的内存/显存（同时加载两个模型），每一步的时间大约翻倍。"}
        }},
        {"use_reviewer_label", {
            {Language::EN, "Use a second model to review each file"}, {Language::PT_BR, "Usar um segundo modelo para revisar cada arquivo"},
            {Language::ES, "Usar un segundo modelo para revisar cada archivo"}, {Language::ZH, "使用第二个模型审核每个文件"}
        }},
        {"use_custom_reviewer_prompt_label", {
            {Language::EN, "Use a custom system prompt for the Reviewer"}, {Language::PT_BR, "Usar um prompt de sistema personalizado para o Revisor"},
            {Language::ES, "Usar un prompt de sistema personalizado para el Revisor"}, {Language::ZH, "为审核模型使用自定义系统提示词"}
        }},
        {"custom_reviewer_prompt_hint", {
            {Language::EN, "This replaces the built-in reviewer instructions completely - make sure it still tells the model to respond with exactly \"APPROVED\" when there are no issues."},
            {Language::PT_BR, "Isso substitui completamente as instruções padrão do revisor - garanta que ainda diga pro modelo responder exatamente \"APPROVED\" quando não houver problemas."},
            {Language::ES, "Esto reemplaza completamente las instrucciones predeterminadas del revisor - asegúrate de que aún le diga al modelo que responda exactamente \"APPROVED\" cuando no haya problemas."},
            {Language::ZH, "这将完全替换内置的审核指令——请确保仍然告诉模型在没有问题时准确回复 \"APPROVED\"。"}
        }},
        {"default_reviewer_prompt_note", {
            {Language::EN, "Using the built-in default reviewer prompt (checks for bugs, cross-file inconsistencies, and completeness)."},
            {Language::PT_BR, "Usando o prompt padrão do revisor (verifica bugs, inconsistências entre arquivos, e se está completo)."},
            {Language::ES, "Usando el prompt predeterminado del revisor (verifica errores, inconsistencias entre archivos, y si está completo)."},
            {Language::ZH, "使用内置默认审核提示词（检查错误、跨文件不一致，以及完整性）。"}
        }},
        {"edit_template_button", {
            {Language::EN, "Edit"}, {Language::PT_BR, "Editar"},
            {Language::ES, "Editar"}, {Language::ZH, "编辑"}
        }},
        {"save_changes_button", {
            {Language::EN, "Save changes"}, {Language::PT_BR, "Salvar alterações"},
            {Language::ES, "Guardar cambios"}, {Language::ZH, "保存更改"}
        }},
        {"cancel_button", {
            {Language::EN, "Cancel"}, {Language::PT_BR, "Cancelar"},
            {Language::ES, "Cancelar"}, {Language::ZH, "取消"}
        }},
        {"template_updated_msg", {
            {Language::EN, "Template updated!"}, {Language::PT_BR, "Template atualizado!"},
            {Language::ES, "¡Plantilla actualizada!"}, {Language::ZH, "模板已更新！"}
        }},
        {"edit_builtin_note", {
            {Language::EN, "This is a built-in template: saving creates an editable copy and hides the original (restorable later)."},
            {Language::PT_BR, "Este é um template embutido: salvar cria uma cópia editável e oculta o original (restaurável depois)."},
            {Language::ES, "Esta es una plantilla integrada: guardar crea una copia editable y oculta la original (restaurable después)."},
            {Language::ZH, "这是内置模板：保存会创建可编辑副本并隐藏原始模板（之后可恢复）。"}
        }},
        {"tab_conversations", {
            {Language::EN, "Conversations"}, {Language::PT_BR, "Conversas"},
            {Language::ES, "Conversaciones"}, {Language::ZH, "会话"}
        }},
        {"agent_web_search_tooltip", {
            {Language::EN, "When starting a task, search DuckDuckGo for the task text and give the results to the agent. Useful for tasks that depend on current information (APIs, library versions, etc)."},
            {Language::PT_BR, "Ao iniciar uma tarefa, pesquisa o texto da tarefa no DuckDuckGo e entrega os resultados ao agente. Útil para tarefas que dependem de informação atual (APIs, versões de bibliotecas, etc)."},
            {Language::ES, "Al iniciar una tarea, busca el texto de la tarea en DuckDuckGo y entrega los resultados al agente. Útil para tareas que dependen de información actual (APIs, versiones de bibliotecas, etc)."},
            {Language::ZH, "开始任务时，在 DuckDuckGo 搜索任务文本并将结果提供给代理。适用于依赖最新信息的任务（API、库版本等）。"}
        }},
        {"web_search_checkbox", {
            {Language::EN, "Search the web"}, {Language::PT_BR, "Pesquisar na web"},
            {Language::ES, "Buscar en la web"}, {Language::ZH, "联网搜索"}
        }},
        {"web_search_tooltip", {
            {Language::EN, "Before answering, search DuckDuckGo and read the top results so the model can use up-to-date information. Sources are cited at the end of the answer."},
            {Language::PT_BR, "Antes de responder, pesquisa no DuckDuckGo e lê os melhores resultados para o modelo usar informação atualizada. As fontes são citadas no fim da resposta."},
            {Language::ES, "Antes de responder, busca en DuckDuckGo y lee los mejores resultados para que el modelo use información actualizada. Las fuentes se citan al final de la respuesta."},
            {Language::ZH, "回答前先在 DuckDuckGo 搜索并阅读最佳结果，让模型使用最新信息。答案末尾会注明来源。"}
        }},
        {"web_searching", {
            {Language::EN, "Searching the web..."}, {Language::PT_BR, "Pesquisando na web..."},
            {Language::ES, "Buscando en la web..."}, {Language::ZH, "正在联网搜索..."}
        }},
        {"web_search_failed", {
            {Language::EN, "Web search returned no results - answering from the model's own knowledge."},
            {Language::PT_BR, "A pesquisa na web não retornou resultados - respondendo com o conhecimento do próprio modelo."},
            {Language::ES, "La búsqueda web no devolvió resultados - respondiendo con el conocimiento del propio modelo."},
            {Language::ZH, "联网搜索没有结果——将使用模型自身的知识回答。"}
        }},
        {"web_sources_label", {
            {Language::EN, "Sources:"}, {Language::PT_BR, "Fontes:"},
            {Language::ES, "Fuentes:"}, {Language::ZH, "来源："}
        }},
        {"theme_toggle_light", {
            {Language::EN, "Light mode"}, {Language::PT_BR, "Modo claro"},
            {Language::ES, "Modo claro"}, {Language::ZH, "浅色模式"}
        }},
        {"theme_toggle_dark", {
            {Language::EN, "Dark mode"}, {Language::PT_BR, "Modo escuro"},
            {Language::ES, "Modo oscuro"}, {Language::ZH, "深色模式"}
        }},
        {"status_prefix", {
            {Language::EN, "Status:"}, {Language::PT_BR, "Status:"},
            {Language::ES, "Estado:"}, {Language::ZH, "状态："}
        }},
        {"status_offline", {
            {Language::EN, "Offline"}, {Language::PT_BR, "Offline"},
            {Language::ES, "Desconectado"}, {Language::ZH, "离线"}
        }},
        {"status_loading", {
            {Language::EN, "Loading"}, {Language::PT_BR, "Carregando"},
            {Language::ES, "Cargando"}, {Language::ZH, "加载中"}
        }},
        {"view_server_log", {
            {Language::EN, "View server log"}, {Language::PT_BR, "Ver log do servidor"},
            {Language::ES, "Ver registro del servidor"}, {Language::ZH, "查看服务器日志"}
        }},
        {"status_online", {
            {Language::EN, "Online"}, {Language::PT_BR, "Online"},
            {Language::ES, "En línea"}, {Language::ZH, "在线"}
        }},
        {"tab_settings", {
            {Language::EN, "Settings"}, {Language::PT_BR, "Configurações"},
            {Language::ES, "Configuración"}, {Language::ZH, "设置"}
        }},
        {"general_settings_title", {
            {Language::EN, "General"}, {Language::PT_BR, "Geral"},
            {Language::ES, "General"}, {Language::ZH, "常规"}
        }},
        {"debate_mode_label", {
            {Language::EN, "Debate Mode"}, {Language::PT_BR, "Modo Debate"},
            {Language::ES, "Modo Debate"}, {Language::ZH, "辩论模式"}
        }},
        {"debate_mode_hint", {
            {Language::EN, "Two or more models discuss your question over several rounds, then one of them writes a final combined answer. Slower and needs more RAM/VRAM (all models loaded at once), but often gives a stronger result than a single model."},
            {Language::PT_BR, "Dois ou mais modelos discutem sua pergunta em várias rodadas, e depois um deles escreve a resposta final combinada. Mais lento e precisa de mais RAM/VRAM (todos os modelos carregados ao mesmo tempo), mas geralmente dá um resultado melhor que um único modelo."},
            {Language::ES, "Dos o más modelos discuten tu pregunta en varias rondas, y luego uno de ellos escribe la respuesta final combinada. Más lento y necesita más RAM/VRAM (todos los modelos cargados a la vez), pero suele dar un resultado mejor que un solo modelo."},
            {Language::ZH, "两个或更多模型对您的问题进行多轮讨论，然后由其中一个模型写出最终的综合答案。速度较慢，需要更多内存/显存（同时加载所有模型），但通常比单一模型效果更好。"}
        }},
        {"debate_rounds_label", {
            {Language::EN, "Rounds:"}, {Language::PT_BR, "Rodadas:"},
            {Language::ES, "Rondas:"}, {Language::ZH, "轮数："}
        }},
        {"debate_rounds_hint", {
            {Language::EN, "How many back-and-forth rounds before the final combined answer. More rounds = better discussion, but slower."},
            {Language::PT_BR, "Quantas rodadas de ida e volta antes da resposta final combinada. Mais rodadas = discussão melhor, mas mais lenta."},
            {Language::ES, "Cuántas rondas de ida y vuelta antes de la respuesta final combinada. Más rondas = mejor discusión, pero más lenta."},
            {Language::ZH, "最终综合答案之前进行多少轮讨论。轮数越多=讨论越充分，但速度越慢。"}
        }},
        {"debater_label", {
            {Language::EN, "Debater"}, {Language::PT_BR, "Debatedor"},
            {Language::ES, "Debatiente"}, {Language::ZH, "辩论者"}
        }},
        {"remove_debater", {
            {Language::EN, "Remove"}, {Language::PT_BR, "Remover"},
            {Language::ES, "Quitar"}, {Language::ZH, "移除"}
        }},
        {"add_model_button", {
            {Language::EN, "+ Add another model"}, {Language::PT_BR, "+ Adicionar outro modelo"},
            {Language::ES, "+ Agregar otro modelo"}, {Language::ZH, "+ 添加另一个模型"}
        }},
        {"extra_args_title", {
            {Language::EN, "Extra llama-server arguments"}, {Language::PT_BR, "Argumentos extras do llama-server"},
            {Language::ES, "Argumentos extra de llama-server"}, {Language::ZH, "llama-server 额外参数"}
        }},
        {"extra_args_hint", {
            {Language::EN, "Advanced: any extra command-line flags typed here are appended to every llama-server this app starts (Chat, Agent, Reviewer, Debaters). Leave empty if you're not sure - the defaults already work well for most setups."},
            {Language::PT_BR, "Avançado: qualquer flag extra digitada aqui é adicionada a todo llama-server que o app iniciar (Chat, Agente, Revisor, Debatedores). Deixe vazio se não tiver certeza - os padrões já funcionam bem na maioria dos casos."},
            {Language::ES, "Avanzado: cualquier flag extra escrita aquí se agrega a cada llama-server que la app inicie (Chat, Agente, Revisor, Debatientes). Déjalo vacío si no estás seguro - los valores predeterminados ya funcionan bien en la mayoría de los casos."},
            {Language::ZH, "高级：此处输入的任何额外命令行参数都会附加到本应用启动的每个 llama-server（对话、代理、审核、辩论者）。如果不确定，请留空——默认设置在大多数情况下已经运行良好。"}
        }},
        {"extra_args_common_flags_title", {
            {Language::EN, "Show common flags"}, {Language::PT_BR, "Mostrar flags comuns"},
            {Language::ES, "Mostrar flags comunes"}, {Language::ZH, "显示常用参数"}
        }},
        {"extra_args_full_list_hint", {
            {Language::EN, "Full list: run \"llama-server.exe --help\" in a terminal from the bin\\ folder, or check the llama.cpp docs online."},
            {Language::PT_BR, "Lista completa: rode \"llama-server.exe --help\" num terminal dentro da pasta bin\\, ou confira a documentação do llama.cpp online."},
            {Language::ES, "Lista completa: ejecuta \"llama-server.exe --help\" en una terminal dentro de la carpeta bin\\, o consulta la documentación de llama.cpp en línea."},
            {Language::ZH, "完整列表：在 bin\\ 文件夹中的终端运行 \"llama-server.exe --help\"，或查看在线的 llama.cpp 文档。"}
        }},
        {"extra_args_example", {
            {Language::EN, "Example: --flash-attn -t 8 --cache-type-k q8_0"},
            {Language::PT_BR, "Exemplo: --flash-attn -t 8 --cache-type-k q8_0"},
            {Language::ES, "Ejemplo: --flash-attn -t 8 --cache-type-k q8_0"},
            {Language::ZH, "示例：--flash-attn -t 8 --cache-type-k q8_0"}
        }},
        {"extra_args_restart_hint", {
            {Language::EN, "Restart any running server for changes here to take effect."},
            {Language::PT_BR, "Reinicie qualquer servidor rodando para essas mudanças terem efeito."},
            {Language::ES, "Reinicia cualquier servidor en ejecución para que estos cambios surtan efecto."},
            {Language::ZH, "重启正在运行的服务器以使更改生效。"}
        }},
        {"stop_all_servers", {
            {Language::EN, "Stop All Servers"}, {Language::PT_BR, "Parar Todos os Servidores"},
            {Language::ES, "Detener Todos los Servidores"}, {Language::ZH, "停止所有服务器"}
        }},
        {"servers_running_suffix", {
            {Language::EN, "running"}, {Language::PT_BR, "ativos"},
            {Language::ES, "activos"}, {Language::ZH, "运行中"}
        }},
        {"tokens_used_label", {
            {Language::EN, "Context used:"}, {Language::PT_BR, "Contexto usado:"},
            {Language::ES, "Contexto usado:"}, {Language::ZH, "已用上下文："}
        }},
        {"tokens_used_suffix", {
            {Language::EN, "tokens (estimated)"}, {Language::PT_BR, "tokens (estimado)"},
            {Language::ES, "tokens (estimado)"}, {Language::ZH, "个令牌（估计）"}
        }},
        {"diff_no_previous_version", {
            {Language::EN, "(Couldn't find a previous version of this file to diff against - showing new content)"},
            {Language::PT_BR, "(Não encontrei uma versão anterior deste arquivo para comparar - mostrando o conteúdo novo)"},
            {Language::ES, "(No se encontró una versión anterior de este archivo para comparar - mostrando el contenido nuevo)"},
            {Language::ZH, "（找不到此文件的先前版本进行比较——显示新内容）"}
        }},
        {"delete_all_button", {
            {Language::EN, "Delete All"}, {Language::PT_BR, "Apagar Todos"},
            {Language::ES, "Eliminar Todos"}, {Language::ZH, "全部删除"}
        }},
        {"delete_selected_button", {
            {Language::EN, "Delete Selected"}, {Language::PT_BR, "Apagar Selecionados"},
            {Language::ES, "Eliminar Seleccionados"}, {Language::ZH, "删除选中项"}
        }},
        {"delete_all_confirm", {
            {Language::EN, "Delete ALL of these? This cannot be undone."},
            {Language::PT_BR, "Apagar TODOS estes? Isso não pode ser desfeito."},
            {Language::ES, "¿Eliminar TODOS estos? Esto no se puede deshacer."},
            {Language::ZH, "删除全部这些？此操作无法撤销。"}
        }},
        {"search_placeholder", {
            {Language::EN, "Search by title"}, {Language::PT_BR, "Buscar por título"},
            {Language::ES, "Buscar por título"}, {Language::ZH, "按标题搜索"}
        }},
        {"export_button", {
            {Language::EN, "Export .md"}, {Language::PT_BR, "Exportar .md"},
            {Language::ES, "Exportar .md"}, {Language::ZH, "导出 .md"}
        }},
        {"debate_cost_warning_prefix", {
            {Language::EN, "Each message will make approximately"}, {Language::PT_BR, "Cada mensagem vai fazer aproximadamente"},
            {Language::ES, "Cada mensaje hará aproximadamente"}, {Language::ZH, "每条消息大约会进行"}
        }},
        {"debate_cost_warning_suffix", {
            {Language::EN, "inference calls (debaters x rounds, plus the final synthesis)."},
            {Language::PT_BR, "chamadas de inferência (debatedores x rodadas, mais a síntese final)."},
            {Language::ES, "llamadas de inferencia (debatientes x rondas, más la síntesis final)."},
            {Language::ZH, "次推理调用（辩论者数 x 轮数，加上最终综合）。"}
        }},
        {"tab_agent", {
            {Language::EN, "Agent"}, {Language::PT_BR, "Agente"},
            {Language::ES, "Agente"}, {Language::ZH, "代理"}
        }},
        {"agent_project_folder_label", {
            {Language::EN, "Project folder (the agent can only touch files inside this folder)"},
            {Language::PT_BR, "Pasta do projeto (o agente só pode mexer em arquivos dentro dela)"},
            {Language::ES, "Carpeta del proyecto (el agente solo puede tocar archivos dentro de ella)"},
            {Language::ZH, "项目文件夹（代理只能操作此文件夹内的文件）"}
        }},
        {"choose_folder", {
            {Language::EN, "Choose Folder"}, {Language::PT_BR, "Escolher Pasta"},
            {Language::ES, "Elegir Carpeta"}, {Language::ZH, "选择文件夹"}
        }},
        {"no_folder_selected", {
            {Language::EN, "No folder selected"}, {Language::PT_BR, "Nenhuma pasta selecionada"},
            {Language::ES, "Ninguna carpeta seleccionada"}, {Language::ZH, "未选择文件夹"}
        }},
        {"agent_free_mode_label", {
            {Language::EN, "Let the agent work freely (no approval needed, still restricted to the project folder)"},
            {Language::PT_BR, "Deixar o agente trabalhar livre (sem pedir autorização, ainda restrito à pasta do projeto)"},
            {Language::ES, "Dejar que el agente trabaje libremente (sin pedir autorización, aún restringido a la carpeta del proyecto)"},
            {Language::ZH, "让代理自由工作（无需批准，仍仅限于项目文件夹）"}
        }},
        {"agent_free_mode_hint", {
            {Language::EN, "Off (default): the agent asks you to approve every file it creates or edits. On: it proceeds on its own — only the sandbox (project folder) limits it, not your review."},
            {Language::PT_BR, "Desligado (padrão): o agente pede sua aprovação antes de criar ou editar cada arquivo. Ligado: ele segue sozinho — só a pasta do projeto o limita, não a sua revisão."},
            {Language::ES, "Apagado (predeterminado): el agente pide tu aprobación antes de crear o editar cada archivo. Encendido: procede solo — solo la carpeta del proyecto lo limita, no tu revisión."},
            {Language::ZH, "关闭（默认）：代理在创建或编辑每个文件前请求您的批准。开启：它会自行继续——只有项目文件夹限制它，而不是您的审核。"}
        }},
        {"agent_task_label", {
            {Language::EN, "What do you want the agent to build?"}, {Language::PT_BR, "O que você quer que o agente construa?"},
            {Language::ES, "¿Qué quieres que el agente construya?"}, {Language::ZH, "您想让代理构建什么？"}
        }},
        {"start_task", {
            {Language::EN, "Start Task"}, {Language::PT_BR, "Iniciar Tarefa"},
            {Language::ES, "Iniciar Tarea"}, {Language::ZH, "开始任务"}
        }},
        {"stop_agent", {
            {Language::EN, "Stop Agent"}, {Language::PT_BR, "Parar Agente"},
            {Language::ES, "Detener Agente"}, {Language::ZH, "停止代理"}
        }},
        {"agent_working_status", {
            {Language::EN, "Agent is working..."}, {Language::PT_BR, "Agente está trabalhando..."},
            {Language::ES, "El agente está trabajando..."}, {Language::ZH, "代理正在工作..."}
        }},
        {"agent_idle_status", {
            {Language::EN, "Idle"}, {Language::PT_BR, "Ocioso"},
            {Language::ES, "Inactivo"}, {Language::ZH, "空闲"}
        }},
        {"approval_needed_title", {
            {Language::EN, "The agent wants to do this — approve?"},
            {Language::PT_BR, "O agente quer fazer isto — aprovar?"},
            {Language::ES, "El agente quiere hacer esto — ¿aprobar?"},
            {Language::ZH, "代理想要执行此操作——是否批准？"}
        }},
        {"approve", {
            {Language::EN, "Approve"}, {Language::PT_BR, "Aprovar"},
            {Language::ES, "Aprobar"}, {Language::ZH, "批准"}
        }},
        {"reject", {
            {Language::EN, "Reject"}, {Language::PT_BR, "Rejeitar"},
            {Language::ES, "Rechazar"}, {Language::ZH, "拒绝"}
        }},
        {"agent_action_create", {
            {Language::EN, "Create file:"}, {Language::PT_BR, "Criar arquivo:"},
            {Language::ES, "Crear archivo:"}, {Language::ZH, "创建文件："}
        }},
        {"agent_action_edit", {
            {Language::EN, "Edit file:"}, {Language::PT_BR, "Editar arquivo:"},
            {Language::ES, "Editar archivo:"}, {Language::ZH, "编辑文件："}
        }},
        {"agent_folder_required_hint", {
            {Language::EN, "Choose a project folder before starting a task."},
            {Language::PT_BR, "Escolha uma pasta de projeto antes de iniciar uma tarefa."},
            {Language::ES, "Elige una carpeta de proyecto antes de iniciar una tarea."},
            {Language::ZH, "开始任务前请选择一个项目文件夹。"}
        }},
        {"agent_log_label", {
            {Language::EN, "Agent activity"}, {Language::PT_BR, "Atividade do agente"},
            {Language::ES, "Actividad del agente"}, {Language::ZH, "代理活动"}
        }},
        {"tab_history", {
            {Language::EN, "History"}, {Language::PT_BR, "Histórico"},
            {Language::ES, "Historial"}, {Language::ZH, "历史记录"}
        }},
        {"new_conversation", {
            {Language::EN, "New Conversation"}, {Language::PT_BR, "Nova Conversa"},
            {Language::ES, "Nueva Conversación"}, {Language::ZH, "新对话"}
        }},
        {"send", {
            {Language::EN, "Send"}, {Language::PT_BR, "Enviar"},
            {Language::ES, "Enviar"}, {Language::ZH, "发送"}
        }},
        {"load", {
            {Language::EN, "Load"}, {Language::PT_BR, "Carregar"},
            {Language::ES, "Cargar"}, {Language::ZH, "加载"}
        }},
        {"current_conversation_label", {
            {Language::EN, "Current conversation:"}, {Language::PT_BR, "Conversa atual:"},
            {Language::ES, "Conversación actual:"}, {Language::ZH, "当前对话："}
        }},
        {"no_conversations_yet", {
            {Language::EN, "No saved conversations yet. Chat a bit and it will show up here automatically."},
            {Language::PT_BR, "Nenhuma conversa salva ainda. Converse um pouco e ela aparece aqui automaticamente."},
            {Language::ES, "Aún no hay conversaciones guardadas. Chatea un poco y aparecerá aquí automáticamente."},
            {Language::ZH, "还没有保存的对话。聊一会儿后会自动显示在这里。"}
        }},
        {"delete_conversation_confirm", {
            {Language::EN, "Delete this conversation? This cannot be undone."},
            {Language::PT_BR, "Excluir esta conversa? Isso não pode ser desfeito."},
            {Language::ES, "¿Eliminar esta conversación? Esto no se puede deshacer."},
            {Language::ZH, "删除此对话？此操作无法撤销。"}
        }},
        {"copy_all", {
            {Language::EN, "Copy all"}, {Language::PT_BR, "Copiar tudo"},
            {Language::ES, "Copiar todo"}, {Language::ZH, "复制全部"}
        }},
        {"restore_builtin_templates", {
            {Language::EN, "Restore deleted built-in templates"}, {Language::PT_BR, "Restaurar templates padrão excluídos"},
            {Language::ES, "Restaurar plantillas incorporadas eliminadas"}, {Language::ZH, "恢复已删除的内置模板"}
        }},
        {"you_label", {
            {Language::EN, "You"}, {Language::PT_BR, "Você"},
            {Language::ES, "Tú"}, {Language::ZH, "您"}
        }},
        {"assistant_label", {
            {Language::EN, "Assistant"}, {Language::PT_BR, "Assistente"},
            {Language::ES, "Asistente"}, {Language::ZH, "助手"}
        }},
        {"context_hint", {
            {Language::EN, "The full conversation is sent to the model on every message, so it remembers earlier turns."},
            {Language::PT_BR, "A conversa inteira é enviada ao modelo a cada mensagem, então ele lembra das falas anteriores."},
            {Language::ES, "Toda la conversación se envía al modelo en cada mensaje, así que recuerda los turnos anteriores."},
            {Language::ZH, "每条消息都会将完整对话发送给模型，因此它能记住之前的内容。"}
        }},
        {"add_template_title", {
            {Language::EN, "Add a new template"}, {Language::PT_BR, "Adicionar novo template"},
            {Language::ES, "Agregar nueva plantilla"}, {Language::ZH, "添加新模板"}
        }},
        {"template_name_label", {
            {Language::EN, "Name"}, {Language::PT_BR, "Nome"},
            {Language::ES, "Nombre"}, {Language::ZH, "名称"}
        }},
        {"template_keywords_label", {
            {Language::EN, "Trigger keywords (comma-separated)"}, {Language::PT_BR, "Palavras-chave de ativação (separadas por vírgula)"},
            {Language::ES, "Palabras clave de activación (separadas por comas)"}, {Language::ZH, "触发关键词（逗号分隔）"}
        }},
        {"template_keywords_hint", {
            {Language::EN, "e.g.: rust, cargo, .rs — whenever your prompt contains one of these words, this template is auto-selected."},
            {Language::PT_BR, "ex.: rust, cargo, .rs — sempre que seu prompt contiver uma dessas palavras, este template é selecionado automaticamente."},
            {Language::ES, "ej.: rust, cargo, .rs — cuando tu prompt contenga una de estas palabras, esta plantilla se selecciona automáticamente."},
            {Language::ZH, "例如：rust, cargo, .rs — 只要您的提示词包含其中一个词，就会自动选择此模板。"}
        }},
        {"template_prompt_label", {
            {Language::EN, "System prompt"}, {Language::PT_BR, "Prompt de sistema"},
            {Language::ES, "Prompt de sistema"}, {Language::ZH, "系统提示词"}
        }},
        {"add_template_button", {
            {Language::EN, "Add template"}, {Language::PT_BR, "Adicionar template"},
            {Language::ES, "Agregar plantilla"}, {Language::ZH, "添加模板"}
        }},
        {"custom_badge", {
            {Language::EN, "custom"}, {Language::PT_BR, "seu"},
            {Language::ES, "tuya"}, {Language::ZH, "自定义"}
        }},
        {"builtin_badge", {
            {Language::EN, "built-in"}, {Language::PT_BR, "pronto"},
            {Language::ES, "incorporada"}, {Language::ZH, "内置"}
        }},
        {"delete_template_confirm", {
            {Language::EN, "Delete this template?"}, {Language::PT_BR, "Excluir este template?"},
            {Language::ES, "¿Eliminar esta plantilla?"}, {Language::ZH, "删除此模板？"}
        }},
        {"template_added_msg", {
            {Language::EN, "Template added."}, {Language::PT_BR, "Template adicionado."},
            {Language::ES, "Plantilla agregada."}, {Language::ZH, "模板已添加。"}
        }},
        {"tab_chat", {
            {Language::EN, "Chat"}, {Language::PT_BR, "Conversa"},
            {Language::ES, "Chat"}, {Language::ZH, "对话"}
        }},
        {"tab_models", {
            {Language::EN, "Models"}, {Language::PT_BR, "Modelos"},
            {Language::ES, "Modelos"}, {Language::ZH, "模型"}
        }},
        {"settings", {
            {Language::EN, "Settings"}, {Language::PT_BR, "Configurações"},
            {Language::ES, "Configuración"}, {Language::ZH, "设置"}
        }},
        {"dark_mode", {
            {Language::EN, "Dark Mode"}, {Language::PT_BR, "Modo Escuro"},
            {Language::ES, "Modo Oscuro"}, {Language::ZH, "深色模式"}
        }},
        {"language", {
            {Language::EN, "Language"}, {Language::PT_BR, "Idioma"},
            {Language::ES, "Idioma"}, {Language::ZH, "语言"}
        }},
        {"thinking_mode", {
            {Language::EN, "Enable Thinking Mode"}, {Language::PT_BR, "Ativar Modo de Pensamento"},
            {Language::ES, "Activar Modo de Pensamiento"}, {Language::ZH, "启用思考模式"}
        }},
        {"prompt_label", {
            {Language::EN, "Prompt"}, {Language::PT_BR, "Texto de Entrada"},
            {Language::ES, "Texto de Entrada"}, {Language::ZH, "输入提示"}
        }},
        {"output_label", {
            {Language::EN, "Result"}, {Language::PT_BR, "Resultado"},
            {Language::ES, "Resultado"}, {Language::ZH, "结果"}
        }},
        {"thinking_label", {
            {Language::EN, "Model Thinking (live)"}, {Language::PT_BR, "Pensamento do Modelo (ao vivo)"},
            {Language::ES, "Pensamiento del Modelo (en vivo)"}, {Language::ZH, "模型思考过程（实时）"}
        }},
        {"generate", {
            {Language::EN, "Generate"}, {Language::PT_BR, "Gerar"},
            {Language::ES, "Generar"}, {Language::ZH, "生成"}
        }},
        {"stop", {
            {Language::EN, "Stop"}, {Language::PT_BR, "Parar"},
            {Language::ES, "Detener"}, {Language::ZH, "停止"}
        }},
        {"stop_server", {
            {Language::EN, "Stop Server"}, {Language::PT_BR, "Parar Servidor"},
            {Language::ES, "Detener Servidor"}, {Language::ZH, "停止服务器"}
        }},
        {"status_idle", {
            {Language::EN, "Idle"}, {Language::PT_BR, "Ocioso"},
            {Language::ES, "Inactivo"}, {Language::ZH, "空闲"}
        }},
        {"status_generating", {
            {Language::EN, "Generating..."}, {Language::PT_BR, "Gerando..."},
            {Language::ES, "Generando..."}, {Language::ZH, "生成中..."}
        }},
        {"status_server_off", {
            {Language::EN, "Server offline"}, {Language::PT_BR, "Servidor desligado"},
            {Language::ES, "Servidor apagado"}, {Language::ZH, "服务器离线"}
        }},
        {"model_label", {
            {Language::EN, "Model (.gguf path)"}, {Language::PT_BR, "Modelo (caminho .gguf)"},
            {Language::ES, "Modelo (ruta .gguf)"}, {Language::ZH, "模型（.gguf 路径）"}
        }},
        {"start_server", {
            {Language::EN, "Start Server"}, {Language::PT_BR, "Iniciar Servidor"},
            {Language::ES, "Iniciar Servidor"}, {Language::ZH, "启动服务器"}
        }},
        {"hf_repo_label", {
            {Language::EN, "Hugging Face repo (e.g. TheBloke/Llama-2-7B-Chat-GGUF)"},
            {Language::PT_BR, "Repositório Hugging Face (ex: TheBloke/Llama-2-7B-Chat-GGUF)"},
            {Language::ES, "Repositorio Hugging Face (ej: TheBloke/Llama-2-7B-Chat-GGUF)"},
            {Language::ZH, "Hugging Face 仓库（例如 TheBloke/Llama-2-7B-Chat-GGUF）"}
        }},
        {"hf_token_label", {
            {Language::EN, "Your private Hugging Face token (for gated/private models)"},
            {Language::PT_BR, "Seu token privado do Hugging Face (para modelos restritos/privados)"},
            {Language::ES, "Tu token privado de Hugging Face (para modelos restringidos/privados)"},
            {Language::ZH, "您的 Hugging Face 私有令牌（用于受限/私有模型）"}
        }},
        {"hf_token_save_hint", {
            {Language::EN, "Token is saved locally in config.json and reused automatically."},
            {Language::PT_BR, "O token é salvo localmente em config.json e reutilizado automaticamente."},
            {Language::ES, "El token se guarda localmente en config.json y se reutiliza automáticamente."},
            {Language::ZH, "令牌保存在本地 config.json 中，将自动重复使用。"}
        }},
        {"save_token", {
            {Language::EN, "Save Token"}, {Language::PT_BR, "Salvar Token"},
            {Language::ES, "Guardar Token"}, {Language::ZH, "保存令牌"}
        }},
        {"clear_token", {
            {Language::EN, "Clear Token"}, {Language::PT_BR, "Limpar Token"},
            {Language::ES, "Borrar Token"}, {Language::ZH, "清除令牌"}
        }},
        {"token_saved_msg", {
            {Language::EN, "Token saved."}, {Language::PT_BR, "Token salvo."},
            {Language::ES, "Token guardado."}, {Language::ZH, "令牌已保存。"}
        }},
        {"fetch_files", {
            {Language::EN, "Fetch .gguf files"}, {Language::PT_BR, "Buscar arquivos .gguf"},
            {Language::ES, "Buscar archivos .gguf"}, {Language::ZH, "获取 .gguf 文件"}
        }},
        {"fetching", {
            {Language::EN, "Fetching..."}, {Language::PT_BR, "Buscando..."},
            {Language::ES, "Buscando..."}, {Language::ZH, "获取中..."}
        }},
        {"download", {
            {Language::EN, "Download"}, {Language::PT_BR, "Baixar"},
            {Language::ES, "Descargar"}, {Language::ZH, "下载"}
        }},
        {"cancel", {
            {Language::EN, "Cancel"}, {Language::PT_BR, "Cancelar"},
            {Language::ES, "Cancelar"}, {Language::ZH, "取消"}
        }},
        {"delete", {
            {Language::EN, "Delete"}, {Language::PT_BR, "Excluir"},
            {Language::ES, "Eliminar"}, {Language::ZH, "删除"}
        }},
        {"downloaded_models", {
            {Language::EN, "Downloaded models"}, {Language::PT_BR, "Modelos baixados"},
            {Language::ES, "Modelos descargados"}, {Language::ZH, "已下载的模型"}
        }},
        {"use_model", {
            {Language::EN, "Use"}, {Language::PT_BR, "Usar"},
            {Language::ES, "Usar"}, {Language::ZH, "使用"}
        }},
        {"select_file_prompt", {
            {Language::EN, "Select a file to download:"},
            {Language::PT_BR, "Selecione um arquivo para baixar:"},
            {Language::ES, "Selecciona un archivo para descargar:"},
            {Language::ZH, "选择要下载的文件："}
        }},
        {"no_files_found", {
            {Language::EN, "No .gguf files found (check repo name or token access)."},
            {Language::PT_BR, "Nenhum arquivo .gguf encontrado (confira o nome do repo ou acesso do token)."},
            {Language::ES, "No se encontraron archivos .gguf (verifica el nombre del repo o el acceso del token)."},
            {Language::ZH, "未找到 .gguf 文件（请检查仓库名称或令牌权限）。"}
        }},
        {"gated_model_hint", {
            {Language::EN, "If this is a gated model, accept the license on huggingface.co first, then paste your token above."},
            {Language::PT_BR, "Se este for um modelo restrito, aceite a licença no huggingface.co primeiro, depois cole seu token acima."},
            {Language::ES, "Si es un modelo restringido, acepta la licencia en huggingface.co primero, luego pega tu token arriba."},
            {Language::ZH, "如果是受限模型，请先在 huggingface.co 上接受许可协议，然后在上方粘贴您的令牌。"}
        }},
        {"active_model_label", {
            {Language::EN, "Active model:"}, {Language::PT_BR, "Modelo ativo:"},
            {Language::ES, "Modelo activo:"}, {Language::ZH, "当前模型："}
        }},
    };
};
