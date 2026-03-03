[🇬🇧 Read in English](README.md)  
   
**D3D9 Depth Proxy**  

Прокси-реализация d3d9.dll для приложений на Direct3D 9.  
Модуль оборачивает интерфейсы IDirect3D9 и IDirect3DDevice9,  
   
 перехватывая вызовы между приложением и реальным D3D9 backend'ом.  
   
 Поведение приложения не изменяется --- прокси работает как прозрачный  
   
 промежуточный слой.  
Проект предназначен для:  
- исследования рендера D3D9 приложений\  
- логирования графических вызовов\  
- внедрения пост-эффектов\  
- работы с depth buffer\  
- анализа графического пайплайна  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAALUlEQVR4nO3OQQ0AIAwEsAMlSJ0UrOFkGngRklZBR1WtJDsAAPzizNcDAADuNcKwAyU+nb+5AAAAAElFTkSuQmCC)  
**Архитектура**  
Приложение  
   
 ↓  
   
 d3d9.dll (proxy)  
   
 ↓  
   
 Реальный D3D9 backend (System / DXVK / Gallium Nine)  
   
 ↓  
   
 GPU  
Реализованы два прокси-класса:  
- D3D9Proxy --- обёртка над IDirect3D9  
- D3D9DeviceProxy --- обёртка над IDirect3DDevice9 (полный  
   
 интерфейс)  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OMQ2AABAAsSNBACP6MMH6NpGACyywEZJWQZeZ2aszAAD+4l6rrTq+ngAA8Nr1AL+6BElk4wV6AAAAAElFTkSuQmCC)  
**Загрузка backend'а**  
При вызове Direct3DCreate9:  
1. Пытается загрузить d3d9_dxvk.dll (для Wine + DXVK)  
2. Если не найден --- загружает системный d3d9.dll  
Один и тот же модуль может использоваться:  
- в Windows (native D3D9)  
- в Wine + DXVK  
- в Wine + Gallium Nine  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNBCkJfE1pYGfHAiAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrse4dwF6o2O55YAAAAASUVORK5CYII=)  
**Возможности**  
**Windows (native D3D9)**  
- Прозрачный перехват всех D3D9 вызовов\  
- Логирование PresentationParameters и render-pass'ов\  
- Захват depth buffer через StretchRect\  
- Поддержка форматов DF24 / DF16 / INTZ\  
- Доступ к depth texture перед Present()\  
- Возможность внедрения пост-эффектов\  
- Overlay-рендер поверх кадра\  
- Перехват CreateTexture и shader-методов\  
- Сохранение color render target через GetRenderTargetData  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANElEQVR4nO3OQQmAUBBAwSf8GGLWDWFDY3ixgjcRZhLMNjNHdQYAwF9cq1rV/vUEAIDX7gcRXAQ2s/16gwAAAABJRU5ErkJggg==)  
**Wine + DXVK**  
- Полный passthrough\  
- Логирование render target переключений\  
- Создание совместимой depth texture\  
- Чтение color render target  
⚠ Захват depth через StretchRect недоступен.  
Причина: DXVK транслирует D3D9 в Vulkan, а копирование depth surface  
   
 через StretchRect не реализовано по архитектурным причинам.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OMQ2AABAAsSNBACPq8MH2NpGACyywEZJWQZeZ2aszAAD+4l6rrTq+ngAA8Nr1AL/KBEe6dElaAAAAAElFTkSuQmCC)  
**Wine + Gallium Nine**  
- Захват depth работает\  
- depth_captured == true после первого геометрического прохода\  
- Depth texture доступна для использования в шейдерах\  
- Поддержка DF24 (AMD) и INTZ (NVIDIA)  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSPBCj5fFyM6mJHAjAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrsexOEF35f1aEgAAAAASUVORK5CYII=)  
**Ограничения**  
Функция                    Windows   DXVK   Gallium Nine  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AUBBAsUeCE4yeIiT9CRVMWGAjJK2CbjNzVGcAAPzF2qu7Wl9PAAB47XoA/vcF8exqpY4AAAAASUVORK5CYII=)  
Passthrough                ✅        ✅     ✅  
   
 Логирование                ✅        ✅     ✅  
   
 Захват depth               ✅        ❌     ✅  
   
 Color screenshot           ✅        ✅     ✅  
   
 Пост-эффекты через depth   ✅        ❌     ✅  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAM0lEQVR4nO3OUQmAABBAsaeI2MKqV8RyJrGCfyJsCbbMzFldAQDwF/dWrdXx9QQAgNf2B/NkAzRb7P0YAAAAAElFTkSuQmCC)  
**Установка**  
**Windows**  
Поместить d3d9.dll **рядом с .exe файлом игры**.  
Прокси автоматически загрузит системный D3D9 backend.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNhQAQ60PcrIhnxgQU2QtIq6DIze3UGAMBf3Gu1VcfXEwAAXrseS14EKxPCORkAAAAASUVORK5CYII=)  
**Linux + Wine + DXVK**  
1. Переименовать оригинальный DXVK d3d9.dll в d3d9_dxvk.dll  
2. Поместить прокси d3d9.dll рядом с .exe файлом игры  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNhZscYahheJwqQgQU2QtIq6DIze3UGAMBf3Gu1VcfXEwAAXrseoqcEQXyAWBgAAAAASUVORK5CYII=)  
**Linux + Wine + Gallium Nine**  
Поместить прокси d3d9.dll рядом с .exe файлом игры.  
Wine автоматически использует Gallium Nine как backend.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OQQmAABRAsSfYxZo/jzlMYQLPJrCCNxG2BFtmZquOAAD4i3Ot7mr/egIAwGvXA4q7Bc870TqdAAAAAElFTkSuQmCC)  
**Сборка**  
**Linux (MinGW → Windows DLL)**  
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared     -o d3d9.dll d3d9_proxy.cpp     -ld3d9 -ldxguid     -static-libgcc -static-libstdc++     -Wl,--kill-at  
   
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSPBCUbfEm6YmFDBhAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrse/w8F7pbTa1oAAAAASUVORK5CYII=)  
**Windows (MSVC)**  
Платформа: **x86**  
Дополнительные зависимости:  
d3d9.lib  
   
 dxguid.lib  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANElEQVR4nO3OQQmAABRAsad4EEtY9QcxnUms4E2ELcGWmTmrKwAA/uLeqrU6vp4AAPDa/gDzXgM37EF77AAAAABJRU5ErkJggg==)  
**Добавление пост-эффектов**  
Точка интеграции находится в методе:  
HRESULT STDMETHODCALLTYPE Present(...) override  
   
Если depth_captured == true, то depth_tex содержит depth буфер  
   
 текущего кадра  
   
 (Windows / Gallium Nine).  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OQQmAABRAsSd4NIGRTPXNaQBrWMGbCFuCLTOzV2cAAPzFvVZbdXw9AQDgtesBhZQEOYZGgUEAAAAASUVORK5CYII=)  
**Назначение проекта**  
Проект представляет собой вспомогательный прокси-модуль для анализа и  
   
 расширения поведения Direct3D 9 приложений.  
