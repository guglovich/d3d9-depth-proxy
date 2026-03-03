[🇬🇧 English version](README.md)

# D3D9 Depth Proxy

Прокси-реализация `d3d9.dll` для приложений Direct3D 9.

Модуль оборачивает интерфейсы `IDirect3D9` и `IDirect3DDevice9`, перехватывая вызовы между приложением и реальным D3D9-бэкендом.  
Поведение приложения не изменяется — прокси работает как прозрачный промежуточный слой.

## Назначение

- Исследование рендера D3D9
- Логирование графических вызовов
- Интеграция пост-эффектов
- Доступ к depth-буферу
- Анализ графического пайплайна

## Архитектура

`Приложение → d3d9.dll (proxy) → Реальный D3D9 backend (System / DXVK / Gallium Nine) → GPU`

Реализованы два класса:

- `D3D9Proxy` — обёртка над `IDirect3D9`
- `D3D9DeviceProxy` — обёртка над полным интерфейсом `IDirect3DDevice9`

## Загрузка бэкенда

При вызове `Direct3DCreate9`:

1. Попытка загрузить `d3d9_dxvk.dll` (Wine + DXVK)
2. Если отсутствует — загружается системный `d3d9.dll`

Одна сборка работает на:

- Windows
- Wine + DXVK
- Wine + Gallium Nine

## Возможности

| Функция                 | Windows | DXVK | Gallium Nine |
|-------------------------|---------|------|--------------|
| Прозрачный passthrough  | ✅      | ✅   | ✅           |
| Логирование             | ✅      | ✅   | ✅           |
| Захват depth            | ✅      | ❌   | ✅           |
| Захват color RT         | ✅      | ✅   | ✅           |
| Depth-based post FX     | ✅      | ❌   | ✅           |

DXVK не поддерживает копирование depth через `StretchRect` из-за ограничений слоя трансляции Vulkan.

---

## Установка

Положить прокси `d3d9.dll` рядом с исполняемым файлом игры.

**Wine + DXVK:**
```
Переименовать DXVK:  d3d9.dll → d3d9_dxvk.dll
Положить прокси:     d3d9.dll  (этот проект)
```

**Wine + Gallium Nine:**
```
Положить прокси:     d3d9.dll  (этот проект)
Wine автоматически использует Gallium Nine как бэкенд.
```

**Windows нативный:**
```
Положить прокси:     d3d9.dll  (этот проект)
Прокси автоматически загрузит C:\Windows\System32\d3d9.dll
```

---

## Точка интеграции

Пост-обработка подключается в методе `Present()`:

```cpp
HRESULT STDMETHODCALLTYPE Present(...) override {
    // Когда depth_captured == true, depth_tex содержит depth-буфер текущего кадра.
    // Доступно на: Windows, Gallium Nine.
    // Недоступно на: DXVK.

    return real->Present(a, b, c, d);
}
```

Захват depth происходит автоматически в `SetRenderTarget` при детектировании перехода offscreen → backbuffer. В этот момент depth содержит геометрию мира, UI ещё не нарисован.

---

## Сборка

### Linux (CMake + кросс-компиляция MinGW)

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw32.cmake
make
```

### Linux (одной командой)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll src/d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

### Windows (CMake — Visual Studio)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

### Windows (MSVC — вручную)

1. Открыть Visual Studio 2019 или 2022
2. Создать проект типа **Dynamic-Link Library (DLL)**
3. Добавить `src/d3d9_proxy.cpp`
4. Выбрать платформу **x86 (Win32)**
5. Project Properties → Linker → Input → Additional Dependencies:

```
d3d9.lib
dxguid.lib
```

6. Собрать конфигурацию **Release x86**

### Windows (MinGW)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll src/d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

---

*Проект изначально прототипирован с использованием AI-assisted инструментов разработки.*
