CREATE TABLE IF NOT EXISTS users (
  id BIGINT NOT NULL AUTO_INCREMENT,
  username VARCHAR(32) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  role ENUM('user', 'admin', 'super_admin') NOT NULL DEFAULT 'user',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_username (username),
  KEY idx_users_role (role)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sessions (
  id BIGINT NOT NULL AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  token_hash CHAR(64) NOT NULL,
  expires_at TIMESTAMP NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  revoked_at TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uk_sessions_token_hash (token_hash),
  KEY idx_sessions_user_id (user_id),
  KEY idx_sessions_expires_at (expires_at),
  CONSTRAINT fk_sessions_user_id
    FOREIGN KEY (user_id) REFERENCES users(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS problems (
  id BIGINT NOT NULL AUTO_INCREMENT,
  title VARCHAR(128) NOT NULL,
  statement TEXT NOT NULL,
  input_description TEXT NOT NULL,
  output_description TEXT NOT NULL,
  samples_json JSON NOT NULL,
  difficulty ENUM('easy', 'medium', 'hard') NOT NULL,
  tags_json JSON NOT NULL,
  created_by BIGINT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_problems_title (title),
  KEY idx_problems_difficulty (difficulty),
  KEY idx_problems_created_by (created_by),
  CONSTRAINT fk_problems_created_by
    FOREIGN KEY (created_by) REFERENCES users(id)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS testcases (
  id BIGINT NOT NULL AUTO_INCREMENT,
  problem_id BIGINT NOT NULL,
  case_index INT NOT NULL,
  input_path VARCHAR(512) NOT NULL,
  output_path VARCHAR(512) NOT NULL,
  input_size BIGINT NOT NULL DEFAULT 0,
  output_size BIGINT NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_testcases_problem_case (problem_id, case_index),
  KEY idx_testcases_problem_id (problem_id),
  CONSTRAINT fk_testcases_problem_id
    FOREIGN KEY (problem_id) REFERENCES problems(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS submissions (
  id BIGINT NOT NULL AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  problem_id BIGINT NOT NULL,
  language ENUM('cpp') NOT NULL DEFAULT 'cpp',
  status ENUM(
    'Pending',
    'Compiling',
    'Running',
    'Accepted',
    'WrongAnswer',
    'TimeLimitExceeded',
    'MemoryLimitExceeded',
    'RuntimeError',
    'CompileError',
    'OutputLimitExceeded',
    'SystemError'
  ) NOT NULL DEFAULT 'Pending',
  source_path VARCHAR(512) NOT NULL,
  source_deleted_at TIMESTAMP NULL DEFAULT NULL,
  compile_message TEXT NULL,
  total_time_ms INT NOT NULL DEFAULT 0,
  max_memory_kb INT NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_submissions_user_id (user_id),
  KEY idx_submissions_problem_id (problem_id),
  KEY idx_submissions_status_created (status, created_at),
  CONSTRAINT fk_submissions_user_id
    FOREIGN KEY (user_id) REFERENCES users(id)
    ON DELETE CASCADE,
  CONSTRAINT fk_submissions_problem_id
    FOREIGN KEY (problem_id) REFERENCES problems(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS submission_cases (
  id BIGINT NOT NULL AUTO_INCREMENT,
  submission_id BIGINT NOT NULL,
  case_index INT NOT NULL,
  status ENUM(
    'Accepted',
    'WrongAnswer',
    'TimeLimitExceeded',
    'MemoryLimitExceeded',
    'RuntimeError',
    'OutputLimitExceeded',
    'SystemError'
  ) NOT NULL,
  time_ms INT NOT NULL DEFAULT 0,
  memory_kb INT NOT NULL DEFAULT 0,
  error_type VARCHAR(64) NOT NULL DEFAULT '',
  message TEXT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_submission_cases_submission_case (submission_id, case_index),
  KEY idx_submission_cases_submission_id (submission_id),
  CONSTRAINT fk_submission_cases_submission_id
    FOREIGN KEY (submission_id) REFERENCES submissions(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS judge_tasks (
  id BIGINT NOT NULL AUTO_INCREMENT,
  submission_id BIGINT NOT NULL,
  status ENUM('Pending', 'Claimed', 'Done', 'Failed') NOT NULL DEFAULT 'Pending',
  locked_by VARCHAR(128) NULL DEFAULT NULL,
  locked_at TIMESTAMP NULL DEFAULT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_judge_tasks_submission_id (submission_id),
  KEY idx_judge_tasks_claim (status, created_at),
  CONSTRAINT fk_judge_tasks_submission_id
    FOREIGN KEY (submission_id) REFERENCES submissions(id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
