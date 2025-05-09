-- Создание таблицы проектов
CREATE TABLE project (
    project_id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    template_style TEXT
);

-- Создание таблицы категорий
CREATE TABLE category (
    category_id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    parent_id INT NULL,
    position INT NOT NULL,
    depth INT NOT NULL,
    project_id INT NOT NULL,
    FOREIGN KEY (parent_id) REFERENCES category(category_id) ON DELETE CASCADE,
    FOREIGN KEY (project_id) REFERENCES project(project_id) ON DELETE CASCADE
);

-- Создание таблицы шаблонов
CREATE TABLE template (
    template_id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
	subtitle TEXT,
    category_id INT NOT NULL,
    notes TEXT,
    programming_notes TEXT,
    position INT NOT NULL,
    is_dynamic BOOLEAN,
    template_type TEXT NOT NULL CHECK (template_type IN ('table','listing','graph')),
    FOREIGN KEY (category_id) REFERENCES category(category_id) ON DELETE CASCADE
);

-- Создание объединённой таблицы для ячеек (как заголовочные, так и ячейки содержимого)
CREATE TABLE grid_cells (
    template_id INT NOT NULL,
    cell_type TEXT NOT NULL CHECK (cell_type IN ('header','content')),
    row_index INT NOT NULL,
    col_index INT NOT NULL,
    row_span INT NOT NULL DEFAULT 1,
    col_span INT NOT NULL DEFAULT 1,
    content TEXT,
    colour TEXT,
    PRIMARY KEY (template_id, cell_type, row_index, col_index),
    FOREIGN KEY (template_id) REFERENCES template(template_id) ON DELETE CASCADE
);

-- Создание таблицы графиков
CREATE TABLE graph (
    template_id INT NOT NULL,
    name TEXT,
    graph_type TEXT,
    image BYTEA,
    FOREIGN KEY (template_id) REFERENCES template(template_id) ON DELETE CASCADE
);

-- Создание таблицы библиотеки графиков
CREATE TABLE graph_library (
    name TEXT NOT NULL,
    graph_type TEXT PRIMARY KEY,
    image BYTEA
);
