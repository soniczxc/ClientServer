#include <gtest/gtest.h>
#include "../server/server.cpp"
#include <../httplib.h>
#include <pqxx/pqxx>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QImage>
#include <QBuffer>

void clearLogFile() {
    QFile logFile("../../server/server.log");
    if (logFile.exists()) {
        logFile.remove();
    }
}

void createTestImage(const QString& imageName) {
    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::white);
    image.save("../../server/images/" + imageName);
}

TEST(ServerTests, HandleAddSuccess) {
    clearLogFile();

    httplib::Request req;
    req.body = R"({
        "username": "test_user",
        "password_hash": "test_hash",
        "ФИО": "Иванов Иван Иванович",
        "Группа": "Группа F",
        "Курс": 3,
        "Год": 2023,
        "Фото": "base64encodedimage"
    })";

    httplib::Response res;
    pqxx::connection conn("dbname=testdb user=postgres password=test");

    QString expectedUser = "test_user";
    QString expectedPassword = "test_hash";

    handleAdd(req, res, conn, expectedUser, expectedPassword);

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(res.body, "Record added");

    QFile logFile("../../server/server.log");
    ASSERT_TRUE(logFile.exists());
}

TEST(ServerTests, HandleDeleteSuccess) {
    clearLogFile();

    createTestImage("1.png");

    httplib::Request req;
    req.body = R"({
        "username": "test_user",
        "password_hash": "test_hash",
        "id": 1
    })";

    httplib::Response res;
    pqxx::connection conn("dbname=testdb user=postgres password=test");

    QString expectedUser = "test_user";
    QString expectedPassword = "test_hash";

    handleDelete(req, res, conn, expectedUser, expectedPassword);

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(res.body, "Record deleted");

    QFile imageFile("../../server/images/1.png");
    ASSERT_FALSE(imageFile.exists());

    QFile logFile("../../server/server.log");
    ASSERT_TRUE(logFile.exists());
}

TEST(ServerTests, HandleGetSuccess) {
    clearLogFile();

    httplib::Request req;
    httplib::Response res;
    pqxx::connection conn("dbname=testdb user=postgres password=test");

    handleGet(req, res, conn);

    ASSERT_EQ(res.status, 200);

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(res.body).toUtf8());
    ASSERT_TRUE(jsonDoc.isArray());
    ASSERT_GT(jsonDoc.array().size(), 0);

    QFile logFile("../../server/server.log");
    ASSERT_TRUE(logFile.exists());
}


