#include <iostream>
#include <QCoreApplication>
#include <QSettings>
#include "../httplib.h"
#include <pqxx/pqxx>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QImage>
#include <QBuffer>

using namespace std;

void logMessage(const QString& message) {
    QFile file("../../server/server.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString() << ": " << message << "\n";
    }else{
        qDebug() <<"Не удалось открыть файл логов.";
    }

}
void handleAdd(const httplib::Request& req, httplib::Response& res, pqxx::connection& conn, QString& expectedUser, QString& expectedPassword) {
    try {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(req.body).toUtf8());
        QJsonObject j = jsonDoc.object();
        QString user = j["username"].toString();
        QString hash = j["password_hash"].toString();
        QString name = j["ФИО"].toString();
        QString group = j["Группа"].toString();
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
                image_name = "1.png";
            }else{
                image_name = QString::number(id + 1) + ".png";
            }

            QByteArray byteArray = QByteArray::fromBase64(base64image.toLatin1());
            QFile debugFileAfterDecoding("../../server/images/" + image_name);
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
            logMessage("Запись добавлена.");
        } else {
            res.set_content("Authorization failed. Wrong user or password.", "text/plain");
            logMessage("Ошибка авторизации. Неверные логин или пароль.");
        }
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Ошибка при добавлении записи: " + QString::fromStdString(e.what()));
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
            pqxx::result r = txn.exec("SELECT image FROM students WHERE id = " + txn.quote(id));
            if (!r.empty()) {
                QString image_name = QString::fromStdString(r[0][0].c_str());

                QFile file("../../server/images/" + image_name);
                if (file.exists()) {
                    if (file.remove()) {
                        logMessage("Удалено изображение: " + image_name);
                    } else {
                        logMessage("Не удалось удалить изображение: " + image_name);
                    }
                } else {
                    logMessage("Изображение не найдено: " + image_name);
                }
                txn.exec0("DELETE FROM students WHERE id = " + txn.quote(id));
                txn.commit();
                res.set_content("Record deleted", "text/plain");
                logMessage("Удалена запись ID: " + QString::number(id));
            }
        }
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Ошибка при удалении: " + QString::fromStdString(e.what()));
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
            jsonObj["Группа"] = QString::fromStdString(row[2].c_str());
            jsonObj["Курс"] = row[3].as<int>();
            jsonObj["Год"] = row[4].as<int>();

            QString image_name = QString::fromStdString(row[5].c_str());
            QImage image("../../server/images/" + image_name);
            if (image.isNull()) {
                qWarning("Ошибка при загрузке фото.");
                logMessage("Фото не найдено.");
                return;
            }
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            if (!image.save(&buffer, "PNG")){
                qWarning("Ошибка при сохранении фото.");
                return;
            }

            QString base64Image = QString::fromLatin1(byteArray.toBase64());
            jsonObj["Фото"] = base64Image;

            jsonArray.append(jsonObj);
        }
        QJsonDocument jsonDoc(jsonArray);
        res.set_content(jsonDoc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
        logMessage("Данные отправлены.");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
        logMessage("Ошибка при обновлении данных: " + QString::fromStdString(e.what()));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QSettings settings("../../server/server.ini", QSettings::IniFormat);
    QString dbConnStr = settings.value("Database/Connection").toString();
    int port = settings.value("Server/Port").toInt();
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

        logMessage("Сервер запущен и слушает порт " + QString::number(port));
        qDebug() << "Сервер запущен. Порт " + QString::number(port);
        svr.listen("0.0.0.0", port);

    } catch (const std::exception& e) {
        logMessage("Ошибка запуска сервера: " + QString::fromStdString(e.what()));
        return 1;
    }

    return app.exec();
}
