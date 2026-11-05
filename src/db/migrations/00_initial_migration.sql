-- Enable foreign keys
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS migrations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    idx INTEGER UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS packages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    display_name TEXT NOT NULL,
    cmake_name TEXT,
    description TEXT
);

CREATE TABLE IF NOT EXISTS package_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    package_id INTEGER NOT NULL,
    version TEXT NOT NULL,
    manifest_json TEXT NOT NULL, -- Cache the raw JSON manifest for quick execution
    installed_at TIMESTAMP,
    FOREIGN KEY (package_id) REFERENCES packages(id) ON DELETE CASCADE,
    UNIQUE(package_id, version)
);

CREATE TABLE IF NOT EXISTS package_dependencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    dep_names TEXT NOT NULL,
    dep_version TEXT,
    dep_type TEXT NOT NULL
);

-- Full-Text Search virtual table for instant package discovery
CREATE VIRTUAL TABLE IF NOT EXISTS packages_fts USING fts5(
    name, 
    display_name, 
    cmake_name,
    description
);
