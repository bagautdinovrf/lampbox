# Эмблема установщика

Сохранён исходный знак приложения: лампочка, треугольник воспроизведения и два
штриха основания. Геометрия немного смягчена, пропорции и отступы выровнены.
Основа — [Apple HIG: App icons](https://developer.apple.com/design/human-interface-guidelines/app-icons):
простой узнаваемый символ, центральная композиция, минимум деталей.

`emblem.png` создан встроенным ImageGen редактированием исходной растровой
эмблемы `src/icons/app.ico`, с настоящей прозрачностью. Воспроизводимый экспорт
готовых изображений: `python Installer/prepare-artwork.py` (нужен Pillow).
Обычная сборка установщика использует уже экспортированные файлы.

- `setup.ico` и `src/icons/app.ico`: одинаковые иконки от 16 до 256 пикселей, прозрачный фон.
- `wizard-large.bmp`: 492 × 942, светлый фон; тройной размер стандартной панели.
- `wizard-small.bmp`: 165 × 165, белый фон; тройной размер стандартной эмблемы.
- `emblem-preview.png`: изображение на белом фоне для проверки.

Запрос ImageGen:

> Production installer emblem for MediaBoxManager. Preserve the charcoal lightbulb
> outline, centered orange play triangle, and two rounded base strokes. Refine
> proportions, smooth symmetric curves, align base strokes, consistent readable
> weight, restrained coral-orange accent. Simple centered flat glyph inspired by
> Apple HIG clarity. Actual transparent background, including inside the bulb.
> No tile, text, shadows, textures, branding, or extra elements.
