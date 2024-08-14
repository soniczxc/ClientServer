#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonArray>
#include <QLabel>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QBuffer>
#include <pqxx/binarystring.hxx>

class ClientApp : public QWidget {
Q_OBJECT

public:
    ClientApp(QWidget* parent = nullptr) : QWidget(parent), manager(new QNetworkAccessManager(this)) {
        QSettings settings("../../client/client.ini", QSettings::IniFormat);
        serverUrl = settings.value("Server/Url").toString();
        logFilePath = settings.value("Log/FilePath").toString();
        username = settings.value("Auth/Username").toString();
        QString password = settings.value("Auth/Password").toString();
        password_hash = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());

        QVBoxLayout* layout = new QVBoxLayout(this);

        QHBoxLayout* formLayout = new QHBoxLayout();
        nameEdit = new QLineEdit();
        groupEdit = new QLineEdit();
        courseEdit = new QLineEdit();
        yearEdit = new QLineEdit();

        formLayout->addWidget(new QLabel("ФИО:"));
        formLayout->addWidget(nameEdit);
        formLayout->addWidget(new QLabel("Группа:"));
        formLayout->addWidget(groupEdit);
        formLayout->addWidget(new QLabel("Курс:"));
        formLayout->addWidget(courseEdit);
        formLayout->addWidget(new QLabel("Год:"));
        formLayout->addWidget(yearEdit);

        auto addButton = new QPushButton("Добавить");
        auto deleteButton = new QPushButton("Удалить");
        tableView = new QTableWidget();

        layout->addLayout(formLayout);
        layout->addWidget(addButton);
        layout->addWidget(deleteButton);
        layout->addWidget(tableView);

        connect(addButton, &QPushButton::clicked, this, &ClientApp::addRecord);
        connect(deleteButton, &QPushButton::clicked, this, &ClientApp::deleteRecord);
        connect(manager, &QNetworkAccessManager::finished, this, &ClientApp::onReplyFinished);

        log("Клиент запущен.");
        loadTable();
    }

private slots:
    void addRecord() {
        QString fileName = QFileDialog::getOpenFileName(this, "Выберите изображение");
        if (fileName.isEmpty()) return;

        qDebug() << fileName;
        QImage image(fileName);
        if (image.isNull()) {
            qWarning("Ошибка при загрузке фото.");
            return;
        }
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.save(&buffer, "PNG");

        QString base64Image = QString::fromLatin1(byteArray.toBase64());

        QJsonObject json;
        json["username"] = username;
        json["password_hash"] = password_hash;
        json["ФИО"] = nameEdit->text();
        json["Группа"] = groupEdit->text();
        json["Курс"] = courseEdit->text().toInt();
        json["Год"] = yearEdit->text().toInt();
        json["Фото"] = base64Image;

        QNetworkRequest request(QUrl(serverUrl + "/add"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply* reply = manager->post(request, QJsonDocument(json).toJson());
        connect(reply, &QNetworkReply::finished, this, &ClientApp::onAddReplyFinished);

        log("Добавлена запись.");
    }

    void deleteRecord() {
        int row = tableView->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Удаление", "Выберите запись для удаления.");
            return;
        }

        QJsonObject json;
        json["username"] = username;
        json["password_hash"] = password_hash;
        json["id"] = tableView->item(row, 0)->text().toInt();

        QNetworkRequest request(QUrl(serverUrl + "/delete"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply* reply = manager->post(request, QJsonDocument(json).toJson());
        connect(reply, &QNetworkReply::finished, this, &ClientApp::onDeleteReplyFinished);

        log("Удалена запись с ID: " + tableView->item(row, 0)->text());
    }

    void loadTable() {
        QNetworkRequest request(QUrl(serverUrl + "/data"));
        QNetworkReply* reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, this, &ClientApp::onReplyFinished);

        log("Загружены данные из таблицы.");
    }

    void onReplyFinished() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        tableView->setRowCount(array.size());
        tableView->setColumnCount(6);
        tableView->setHorizontalHeaderLabels({"ID", "ФИО", "Группа", "Курс", "Год", "Фото"});

        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            tableView->setItem(i, 0, new QTableWidgetItem(QString::number(obj["id"].toInt())));
            tableView->setItem(i, 1, new QTableWidgetItem(obj["ФИО"].toString()));
            tableView->setItem(i, 2, new QTableWidgetItem(obj["Группа"].toString()));
            tableView->setItem(i, 3, new QTableWidgetItem(QString::number(obj["Курс"].toInt())));
            tableView->setItem(i, 4, new QTableWidgetItem(QString::number(obj["Год"].toInt())));

            QString base64image = obj["Фото"].toString();
            QByteArray byteArray = QByteArray::fromBase64(base64image.toLatin1());


            QPixmap pixmap;
            if (!pixmap.loadFromData(byteArray)) {
                qDebug() << "Ошибка при загрузке фото";
                pixmap = QPixmap(200, 400);
                pixmap.fill(Qt::gray);
            }

            QPixmap scaledPixmap = pixmap.scaled(100, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);


            QLabel *imageLabel = new QLabel();
            imageLabel->setPixmap(scaledPixmap);
            imageLabel->setScaledContents(true);
            imageLabel->setAlignment(Qt::AlignCenter);
            imageLabel->setFixedSize(scaledPixmap.size());

            tableView->setCellWidget(i, 5, imageLabel);
            tableView->resizeRowsToContents();
            tableView->resizeColumnsToContents();
        }

        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        reply->deleteLater();
    }

    void onAddReplyFinished() {
        QMessageBox::information(this, "Добавление", "Запись добавлена.");
        loadTable();
    }

    void onDeleteReplyFinished() {
        QMessageBox::information(this, "Удаление", "Запись удалена.");
        loadTable();
    }

private:
    void log(const QString& message) {
        QFile file(logFilePath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << QDateTime::currentDateTime().toString() << ": " << message << "\n";
        }
    }

    QLineEdit* nameEdit;
    QLineEdit* groupEdit;
    QLineEdit* courseEdit;
    QLineEdit* yearEdit;
    QTableWidget* tableView;
    QNetworkAccessManager* manager;
    QString serverUrl;
    QString logFilePath;
    QString username;
    QString password_hash;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ClientApp window;
    window.show();
    return app.exec();
}

#include "client.moc"