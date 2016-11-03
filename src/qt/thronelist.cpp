#include "thronelist.h"
#include "ui_thronelist.h"

#include "sync.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "activethrone.h"
#include "throne-sync.h"
#include "throneconfig.h"
#include "throneman.h"
#include "wallet.h"
#include "init.h"
#include "guiutil.h"

#include <QTimer>
#include <QMessageBox>

CCriticalSection cs_thrones;

ThroneList::ThroneList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ThroneList),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyThrones->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyThrones->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyThrones->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyThrones->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyThrones->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyThrones->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetThrones->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetThrones->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetThrones->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetThrones->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetThrones->setColumnWidth(4, columnLastSeenWidth);

    ui->tableWidgetMyThrones->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyThrones, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    updateNodeList();
}

ThroneList::~ThroneList()
{
    delete ui;
}

void ThroneList::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // try to update list when throne count changes
        connect(clientModel, SIGNAL(strThronesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void ThroneList::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void ThroneList::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMyThrones->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void ThroneList::StartAlias(std::string strAlias)
{
    std::string statusObj;
    statusObj += "<center>Alias: " + strAlias;

    BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string errorMessage;
            CThroneBroadcast mnb;

            bool result = activeThrone.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

            if(result) {
                statusObj += "<br>Successfully started throne." ;
                mnodeman.UpdateThroneList(mnb);
                mnb.Relay();
            } else {
                statusObj += "<br>Failed to start throne.<br>Error: " + errorMessage;
            }
            break;
        }
    }
    statusObj += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(statusObj));

    msg.exec();
}

void ThroneList::StartAll(std::string strCommand)
{
    int total = 0;
    int successful = 0;
    int fail = 0;
    std::string statusObj;

    BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
        std::string errorMessage;
        CThroneBroadcast mnb;

        CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        CThrone *pmn = mnodeman.Find(vin);

        if(strCommand == "start-missing" && pmn) continue;

        bool result = activeThrone.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

        if(result) {
            successful++;
            mnodeman.UpdateThroneList(mnb);
            mnb.Relay();
        } else {
            fail++;
            statusObj += "\nFailed to start " + mne.getAlias() + ". Error: " + errorMessage;
        }
        total++;
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d thrones, failed to start %d, total %d", successful, fail, total);
    if (fail > 0)
        returnObj += statusObj;

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();
}

void ThroneList::updateMyThroneInfo(QString alias, QString addr, QString privkey, QString txHash, QString txIndex, CThrone *pmn)
{
    LOCK(cs_mnlistupdate);
    bool bFound = false;
    int nodeRow = 0;

    for(int i=0; i < ui->tableWidgetMyThrones->rowCount(); i++)
    {
        if(ui->tableWidgetMyThrones->item(i, 0)->text() == alias)
        {
            bFound = true;
            nodeRow = i;
            break;
        }
    }

    if(nodeRow == 0 && !bFound) {
        nodeRow = ui->tableWidgetMyThrones->rowCount();
        ui->tableWidgetMyThrones->insertRow(nodeRow);
    }

    QTableWidgetItem *aliasItem = new QTableWidgetItem(alias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(addr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(pmn ? pmn->protocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(pmn ? pmn->Status() : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(pmn ? (pmn->lastPing.sigTime - pmn->sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(GUIUtil::dateTimeStr(pmn ? pmn->lastPing.sigTime : 0));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(pmn ? CBitcoinAddress(pmn->pubkey.GetID()).ToString() : ""));

    ui->tableWidgetMyThrones->setItem(nodeRow, 0, aliasItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 1, addrItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 2, protocolItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 3, statusItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 4, activeSecondsItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 5, lastSeenItem);
    ui->tableWidgetMyThrones->setItem(nodeRow, 6, pubkeyItem);
}

void ThroneList::updateMyNodeList(bool reset) {
    static int64_t lastMyListUpdate = 0;

    // automatically update my throne list only once in MY_THRONELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t timeTillUpdate = lastMyListUpdate + MY_THRONELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(timeTillUpdate));

    if(timeTillUpdate > 0 && !reset) return;
    lastMyListUpdate = GetTime();

    ui->tableWidgetThrones->setSortingEnabled(false);
    BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
        CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        CThrone *pmn = mnodeman.Find(vin);

        updateMyThroneInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), QString::fromStdString(mne.getPrivKey()), QString::fromStdString(mne.getTxHash()),
            QString::fromStdString(mne.getOutputIndex()), pmn);
    }
    ui->tableWidgetThrones->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void ThroneList::updateNodeList()
{
    static int64_t lastListUpdate = 0;

    // update only once in THRONELIST_UPDATE_SECONDS seconds to prevent high cpu usage e.g. on filter change
    if(GetTime() - lastListUpdate < THRONELIST_UPDATE_SECONDS) return;
    lastListUpdate = GetTime();

    TRY_LOCK(cs_thrones, lockThrones);
    if(!lockThrones)
        return;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetThrones->setSortingEnabled(false);
    ui->tableWidgetThrones->clearContents();
    ui->tableWidgetThrones->setRowCount(0);
    std::vector<CThrone> vThrones = mnodeman.GetFullThroneVector();

    BOOST_FOREACH(CThrone& mn, vThrones)
    {
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.protocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.Status()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(GUIUtil::dateTimeStr(mn.lastPing.sigTime));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(CBitcoinAddress(mn.pubkey.GetID()).ToString()));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetThrones->insertRow(0);
        ui->tableWidgetThrones->setItem(0, 0, addressItem);
        ui->tableWidgetThrones->setItem(0, 1, protocolItem);
        ui->tableWidgetThrones->setItem(0, 2, statusItem);
        ui->tableWidgetThrones->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetThrones->setItem(0, 4, lastSeenItem);
        ui->tableWidgetThrones->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetThrones->rowCount()));
    ui->tableWidgetThrones->setSortingEnabled(true);

}

void ThroneList::on_filterLineEdit_textChanged(const QString &filterString) {
    strCurrentFilter = filterString;
    ui->countLabel->setText("Please wait...");
}

void ThroneList::on_startButton_clicked()
{
    // Find selected node alias
    QItemSelectionModel* selectionModel = ui->tableWidgetMyThrones->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string strAlias = ui->tableWidgetMyThrones->item(r, 0)->text().toStdString();

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm throne start"),
        tr("Are you sure you want to start throne %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        return;
    }

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly)
    {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if(!ctx.isValid())
        {
            // Unlock wallet was cancelled
            return;
        }
        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void ThroneList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all thrones start"),
        tr("Are you sure you want to start ALL thrones?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        return;
    }

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly)
    {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if(!ctx.isValid())
        {
            // Unlock wallet was cancelled
            return;
        }
        StartAll();
        return;
    }

    StartAll();
}

void ThroneList::on_startMissingButton_clicked()
{

    if(throneSync.RequestedThroneAssets <= THRONE_SYNC_LIST ||
      throneSync.RequestedThroneAssets == THRONE_SYNC_FAILED) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until throne list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing thrones start"),
        tr("Are you sure you want to start MISSING thrones?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        return;
    }

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly)
    {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if(!ctx.isValid())
        {
            // Unlock wallet was cancelled
            return;
        }
        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void ThroneList::on_tableWidgetMyThrones_itemSelectionChanged()
{
    if(ui->tableWidgetMyThrones->selectedItems().count() > 0)
    {
        ui->startButton->setEnabled(true);
    }
}

void ThroneList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}