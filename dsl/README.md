# Simulink XML to C Code Generator

Генератор C-кода из XML-описания Simulink-моделей. Поддерживает базовые блоки: Inport, Outport, Gain, Sum, UnitDelay.

## Возможности

- Парсинг XML-файлов моделей Simulink
- Генерация ANSI C кода
- Поддержка обратной связи (замкнутый цикл)
- Экспорт портов для внешнего доступа

## Быстрый старт

### Сборка

```bash
mkdir build && cd build
cmake ..
make
```

### Использование
```bash
./generator <input.xml> <model_name>
```

#### Пример:

```bash
./generator pid_controller.xml pid
```

#### Результат
Генератор создает два файла:

- pid_generated.h — заголовочный файл
- pid_generated.c — реализация модели