#include <iostream>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <../cpp-httplib/httplib.h>
#include <pqxx/pqxx>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QImage>
#include <QBuffer>
#include <QPixmap>

using namespace std;

void logMessage(const QString& message) {
    QFile file("/home/vova/CLionProjects/untitled/server/server.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString() << ": " << message << "\n";
    }
}
void setUpDirectory() {
    QString projectDir = QCoreApplication::applicationDirPath();
    QString relativePath = "../../server/images"; // Путь относительно директории сборки
    QDir dir(projectDir);
    QString fullPath = dir.absoluteFilePath(relativePath);

    if (QDir::setCurrent(fullPath)) {
        qDebug() << "Current directory set to:" << QDir::currentPath();
    } else {
        qWarning() << "Failed to change directory to:" << fullPath;
    }
}

void handleAdd(const httplib::Request& req, httplib::Response& res, pqxx::connection& conn, QString& expectedUser, QString& expectedPassword) {
    try {
        setUpDirectory();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(req.body).toUtf8());
        QJsonObject j = jsonDoc.object();
        QString user = j["username"].toString();
        QString hash = j["password_hash"].toString();
        QString name = j["ФИО"].toString();
        QString group = j["Груп"].toString();
        int course = j["Курс"].toInt();
        int year = j["Год"].toInt();
        QString base64image = j["Фото"].toString();
        QString image_name;
        if((user == expectedUser) && (hash == expectedPassword)) {
            pqxx::work txni(conn);
            pqxx::result r = txni.exec("SELECT MAX(ID) FROM students;");
            txni.commit();
            int id = 0;
            if(!r.empty() && !r[0][0].is_null()) {
                id = r[0][0].as<int>();
            }else {
                id = 0;
            }

            if(id == 0) {
                qDebug() << "hueta22";
                image_name = "1.png";
            }else{
                qDebug() << "hueta333";
                image_name = QString::number(id + 1) + ".png";
            }
            qDebug() << image_name;

            QByteArray byteArray = QByteArray::fromBase64(base64image.toLatin1());

            QFile debugFileAfterDecoding(image_name);
            if (debugFileAfterDecoding.open(QIODevice::WriteOnly)) {
                debugFileAfterDecoding.write(byteArray);
                debugFileAfterDecoding.close();
            }
            pqxx::work txn(conn);
            txn.exec0("INSERT INTO students (name, group_name, course, year, image) VALUES (" +
                      txn.quote(name.toStdString()) + ", " +
                      txn.quote(group.toStdString()) + ", " +
                      txn.quote(course) + ", " +
                      txn.quote(year) + ", " +
                      txn.quote(image_name.toStdString()) + ");");
            txn.commit();
            res.set_content("Record added", "text/plain");
            logMessage("Added record: " + QString::fromStdString(req.body));
        } else {
            res.set_content("Authorization failed. Wrong user or password.", "text/plain");
            logMessage("Authorization failed. Wrong user or password.");
        }
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Error adding record: " + QString::fromStdString(e.what()));
    }
}

void handleDelete(const httplib::Request& req, httplib::Response& res, pqxx::connection& conn, QString& expectedUser, QString& expectedPassword) {
    try {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(req.body).toUtf8());
        QJsonObject j = jsonDoc.object();
        QString user = j["username"].toString();
        QString hash = j["password_hash"].toString();
        int id = j["id"].toInt();

        if((user == expectedUser) && (hash == expectedPassword)) {
            pqxx::work txn(conn);
            txn.exec0("DELETE FROM students WHERE id = " + txn.quote(id));
            txn.commit();
            res.set_content("Record deleted", "text/plain");
            logMessage("Deleted record with ID: " + QString::number(id));
        }
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Error deleting record: " + QString::fromStdString(e.what()));
    }
}

void handleGet(const httplib::Request& req, httplib::Response& res, pqxx::connection& conn) {
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec("SELECT * FROM students");
        txn.commit();

        QJsonArray jsonArray;
        for (const auto& row : r) {
            QJsonObject jsonObj;
            jsonObj["id"] = row[0].as<int>();
            jsonObj["ФИО"] = QString::fromStdString(row[1].c_str());
            jsonObj["Груп"] = QString::fromStdString(row[2].c_str());
            jsonObj["Курс"] = row[3].as<int>();
            jsonObj["Год"] = row[4].as<int>();

            QString image_name = QString::fromStdString(row[5].c_str());
            QImage image(image_name);
            if (image.isNull()) {
                qWarning("Failed to load image");
                return;
            }
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            QImage image_out;
            image_out.save(&buffer, "PNG");

            QString base64Image = QString::fromLatin1(byteArray.toBase64());

            jsonObj["Фото"] = base64Image;

            jsonArray.append(jsonObj);
        }

        QJsonDocument jsonDoc(jsonArray);
        res.set_content(jsonDoc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
        logMessage("Data requested.");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Error fetching data: " + QString::fromStdString(e.what()));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QSettings settings("/home/vova/CLionProjects/untitled/server/server.ini", QSettings::IniFormat);
    QString dbConnStr = settings.value("Database/Connection", "dbname=postgres user=postgres password=1234").toString();
    int port = settings.value("Server/Port", 8080).toInt();
    QString expectedUser = settings.value("Auth/Username").toString();
    QString expectedPassword = settings.value("Auth/PasswordHash").toString();

    httplib::Server svr;

    try {
        pqxx::connection conn(dbConnStr.toStdString());
        svr.Post("/add", [&](const httplib::Request& req, httplib::Response& res) {
            handleAdd(req, res, conn, expectedUser, expectedPassword);
        });

        svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) {
            handleDelete(req, res, conn, expectedUser, expectedPassword);
        });

        svr.Get("/data", [&](const httplib::Request& req, httplib::Response& res) {
            handleGet(req, res, conn);
        });

        logMessage("Server is running on port " + QString::number(port));
        svr.listen("0.0.0.0", port);
    } catch (const std::exception& e) {
        logMessage("Error starting server: " + QString::fromStdString(e.what()));
        return 1;
    }

    return app.exec();
}
