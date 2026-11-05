Исправление RefinedBlizzPlates 1.12.0

На обычном клиенте 3.3.5a нет C_NamePlate. Эта версия RefinedBlizzPlates узнаёт
класс союзного игрока из списка группы/рейда, поэтому у остальных союзников
в индикаторах без полос может отсутствовать цвет класса.

RefinedBlizzPlates-1.12.0-class-colors.patch добавляет получение точного класса
через NozdorAction.GetNameplateClass и обновление уже показанных индикаторов.
Требуются DLL и аддон Nozdor Action 0.3.20. Настройки цвета остаются под
контролем RefinedBlizzPlates; без Nozdor Action сохраняется прежнее поведение.

Установленная на этом компьютере версия уже исправлена. Архив Nozdor Action
не заменяет Functions.lua другого аддона автоматически. Для переноса на другой
компьютер с RefinedBlizzPlates 1.12.0 сначала сохрани копию его Functions.lua,
затем из папки игры проверь и примени патч с установленным Git:

    git apply --check client-prototype/compat/RefinedBlizzPlates-1.12.0-class-colors.patch
    git apply client-prototype/compat/RefinedBlizzPlates-1.12.0-class-colors.patch

Если проверка не проходит, файл уже исправлен или отличается от этой версии.
Принудительно накладывать патч на другую версию не следует.

Исходный аддон: https://github.com/KhalGH/RefinedBlizzPlates-WotLK
