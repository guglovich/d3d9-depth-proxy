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

Прокси функционирует как промежуточный слой:

`Приложение → d3d9.dll (proxy) → Реальный D3D9 backend (System / DXVK / Gallium Nine) → GPU`

Реализованы два класса:

- `D3D9Proxy` — обёртка над `IDirect3D9`
- `D3D9DeviceProxy` — обёртка над `IDirect3DDevice9` (полный интерфейс)

## Загрузка backend

При вызове `Direct3DCreate9`:

1. Пытаемся загрузить `d3d9_dxvk.dll`
2. Если отсутствует — загружается системный `d3d9.dll`

Одна сборка может работать в:

- Windows
- Wine + DXVK
- Wine + Gallium Nine

## Ограничения

| Функция                 | Windows | DXVK | Gallium Nine |
|-------------------------|---------|------|--------------|
| Прозрачный passthrough  | ✅      | ✅   | ✅           |
| Логирование             | ✅      | ✅   | ✅           |
| Захват depth            | ✅      | ❌   | ✅           |
| Захват color RT         | ✅      | ✅   | ✅           |
| Depth-based post FX     | ✅      | ❌   | ✅           |

DXVK не поддерживает копирование depth через `StretchRect` из-за ограничений слоя трансляции Vulkan.

---

## Сборка

### Linux (MinGW → Windows DLL)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

### Windows (MSVC — Visual Studio)

1. Открыть Visual Studio 2019 или 2022
2. Создать проект типа **Dynamic-Link Library (DLL)**
3. Добавить `d3d9_proxy.cpp`
4. Выбрать платформу **x86 (Win32)**
5. Project Properties → Linker → Input
6. В *Additional Dependencies* добавить:

```
d3d9.lib
dxguid.lib
```

7. Собрать конфигурацию **Release x86**

### Windows (CMake — Visual Studio)

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

### Windows (MinGW)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

---

## Точка интеграции

Интеграция пост-обработки выполняется в методе:

```cpp
HRESULT STDMETHODCALLTYPE Present(...)
```

Если `depth_captured == true`, переменная `depth_tex` содержит depth-буфер текущего кадра  
(работает на Windows и Gallium Nine).

## Область применения

Лёгкий прокси-модуль для анализа и расширения поведения Direct3D 9 приложений.

Проект был изначально прототипирован с использованием AI-assisted инструментов разработки.