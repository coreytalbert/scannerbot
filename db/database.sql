DROP TABLE recordings;

CREATE TABLE IF NOT EXISTS info(
    id TEXT NOT NULL PRIMARY KEY,
    timestamp TEXT NOT NULL,
    freq TEXT NOT NULL,
    agency TEXT,
    transcript TEXT,
    audioPath TEXT,
    audioDuration TEXT,
    postID TEXT,
    postURL TEXT
);

INSERT INTO
    recordings(
        id,
        Timestamp,
        Duration,
        freq,
        agency,
        filePath,
        fileType,
        audioRes
    )
VALUES
    (
        1,
        1,
        "00:00",
        30,
        400,
        "Test",
        "Test",
        "Test",
        "Test"
    );

SELECT
    *
FROM
    recordings;