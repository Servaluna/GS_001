#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QFile>
#include <QTimer>
#include <QMap>

#include "../localdatabase/localmodels/transferringtask.h"
class ServerConnector;
class LocalDAO;

/**
 * @brief 鏂囦欢浼犺緭绠＄悊鍣?
 *
 * 鑱岃矗锛?
 * 1. 浠庢湇鍔″櫒涓嬭浇鏂囦欢锛堟敮鎸佹柇鐐圭画浼狅級
 * 2. 淇濆瓨鍒版湰鍦扮紦瀛?
 * 3. SHA-256 楠岃瘉
 * 4. 鏇存柊浼犺緭杩涘害鍒版暟鎹簱
 */
class FileTransferManager : public QObject
{
    Q_OBJECT
public:
    explicit FileTransferManager(QObject *parent = nullptr);
    ~FileTransferManager();

    /**
     * @brief 鍒濆鍖栫鐞嗗櫒
     * @param serverConnector 鏈嶅姟鍣ㄨ繛鎺ュ櫒锛堝閮ㄤ紶鍏ワ紝涓嶈礋璐ｆ墍鏈夋潈锛?
     * @param dao 鏁版嵁搴撹闂璞★紙澶栭儴浼犲叆锛?
     * @return 鏄惁鍒濆鍖栨垚鍔?
     */
    bool init(ServerConnector* serverConnector, LocalDAO* dao);

    /**
     * @brief 寮€濮嬩笅杞芥枃浠?
     * @param task 浼犺緭浠诲姟
     * @return 鏄惁鎴愬姛鍚姩涓嬭浇
     */
    bool startDownload(const TransferringTask& task);

    /**
     * @brief 鏆傚仠涓嬭浇
     * @param taskId 浠诲姟ID
     * @return 鏄惁鎴愬姛鏆傚仠
     */
    bool pauseDownload(QString taskId);

    /**
     * @brief 鎭㈠涓嬭浇
     * @param taskId 浠诲姟ID
     * @return 鏄惁鎴愬姛鎭㈠
     */
    bool resumeDownload(QString taskId);

    /**
     * @brief 鍙栨秷涓嬭浇
     * @param taskId 浠诲姟ID
     * @return 鏄惁鎴愬姛鍙栨秷
     */
    bool cancelDownload(QString taskId);

    /**
     * @brief 鑾峰彇涓嬭浇杩涘害
     * @param taskId 浠诲姟ID
     * @return 杩涘害鐧惧垎姣旓紙0-100锛?
     */
    int getProgress(QString taskId) const;

    /**
     * @brief 妫€鏌ユ湰鍦版槸鍚﹀凡鏈夋枃浠?
     * @param taskId 浠诲姟ID
     * @param expectedSha256 鏈熸湜鐨?SHA-256
     * @return 鏂囦欢鏄惁鏈夋晥
     */
    bool isLocalFileValid(QString taskId, const QString& expectedSha256) const;

signals:
    /**
     * @brief 涓嬭浇杩涘害鏇存柊
     * @param taskId 浠诲姟ID
     * @param transferred 宸蹭紶杈撳瓧鑺傛暟
     * @param total 鎬诲瓧鑺傛暟
     * @param progressPercent 杩涘害鐧惧垎姣?
     */
    void progressUpdated(QString taskId, qint64 transferred, qint64 total, int progressPercent);

    /**
     * @brief 涓嬭浇瀹屾垚
     * @param taskId 浠诲姟ID
     * @param localPath 鏈湴鏂囦欢璺緞
     * @param success 鏄惁鎴愬姛锛圫HA-256 楠岃瘉閫氳繃锛?     */
    void downloadFinished(QString taskId, const QString& localPath, bool success);

    /**
     * @brief 涓嬭浇澶辫触
     * @param taskId 浠诲姟ID
     * @param errorCode 閿欒鐮?
     * @param errorMessage 閿欒淇℃伅
     */
    void downloadFailed(QString taskId, int errorCode, const QString& errorMessage);

    /**
     * @brief 涓嬭浇鏆傚仠
     * @param taskId 浠诲姟ID
     */
    void downloadPaused(QString taskId);

    /**
     * @brief 涓嬭浇鎭㈠
     * @param taskId 浠诲姟ID
     */
    void downloadResumed(QString taskId);

private slots:
    /**
     * @brief 澶勭悊鏈嶅姟鍣ㄨ繑鍥炵殑鏂囦欢鍧?
     * @param taskId 浠诲姟ID
     * @param chunkData 鏂囦欢鍧楁暟鎹?
     * @param chunkIndex 鍧楃储寮?
     * @param isLast 鏄惁鏄渶鍚庝竴鍧?
     */
    void onFileChunkReceived(QString taskId, const QByteArray& chunkData, int chunkIndex, bool isLast);

    /**
     * @brief 澶勭悊鏈嶅姟鍣ㄨ繑鍥炵殑鏂囦欢淇℃伅
     * @param taskId 浠诲姟ID
     * @param totalSize 鏂囦欢鎬诲ぇ灏?
     * @param sha256 鏂囦欢 SHA-256
     */
    void onFileInfoReceived(QString taskId, qint64 totalSize, const QString& sha256);

    /**
     * @brief 澶勭悊鏈嶅姟鍣ㄩ敊璇?
     * @param taskId 浠诲姟ID
     * @param errorCode 閿欒鐮?
     * @param errorMessage 閿欒淇℃伅
     */
    void onServerError(QString taskId, int errorCode, const QString& errorMessage);

    /**
     * @brief 瀹氭椂淇濆瓨杩涘害锛堥槻姝㈡暟鎹涪澶憋級
     */
    void onAutoSaveProgress();

private:
    /**
     * @brief 涓嬭浇涓婁笅鏂囷紙璁板綍鍗曚釜涓嬭浇浠诲姟鐨勭姸鎬侊級
     */
    struct DownloadContext {
        // TransferringTask task;

        QString taskId;
        QString localTempPath;      // 涓存椂鏂囦欢璺緞
        QString localCachePath;     // 鏈€缁堟枃浠惰矾寰?
        QString fileName;
        qint64 totalSize;           // 鏂囦欢鎬诲ぇ灏?
        qint64 transferredBytes;    // 宸蹭紶杈撳瓧鑺傛暟
        QString expectedSha256;     // 鏈熸湜鐨?SHA-256
        bool isPaused;              // 鏄惁鏆傚仠
        bool isCancelled;           // 鏄惁鍙栨秷
        QFile* tempFile;            // 涓存椂鏂囦欢鍙ユ焺
        int lastSavedProgress;      // 涓婃淇濆瓨鐨勮繘搴︼紙鐧惧垎姣旓級

        DownloadContext(const TransferringTask& t)
            : taskId(t.task_id)
            ,localTempPath(t.local_temp_path)
            ,localCachePath(t.local_cache_path)
            ,fileName(t.file_name)
            ,totalSize(t.file_size)
            ,transferredBytes(t.transferred_bytes)
            ,expectedSha256(t.file_sha256)

            , isPaused(false)
            , isCancelled(false)
            , tempFile(nullptr)
            , lastSavedProgress(-1) {}

        ~DownloadContext() {
            if (tempFile) {
                tempFile->close();
                delete tempFile;
                tempFile = nullptr;
            }
        }
    };
    /**
     * @brief 璇锋眰涓嬭浇鏂囦欢锛堝彂閫佽姹傚埌鏈嶅姟鍣級
     * @param taskId 浠诲姟ID
     * @param offset 璧峰鍋忕Щ閲忥紙鏂偣缁紶锛?
     * @return 鏄惁鎴愬姛鍙戦€佽姹?
     */
    bool fileDownloadRequest(QString taskId, qint64 offset = 0);

    /**
     * @brief 璁＄畻鏂囦欢 SHA-256
     * @param filePath 鏂囦欢璺緞
     * @return SHA-256 瀛楃涓诧紙灏忓啓锛?     */
    QString calculateFileSha256(const QString& filePath) const;

    /**
     * @brief 淇濆瓨褰撳墠杩涘害鍒版暟鎹簱
     * @param context 涓嬭浇涓婁笅鏂?
     */
    void saveProgressToDatabase(const DownloadContext& context);

    /**
     * @brief 瀹屾垚涓嬭浇锛堥獙璇?SHA-256 骞堕噸鍛藉悕鏂囦欢锛?     * @param context 涓嬭浇涓婁笅鏂?
     * @return 鏄惁鎴愬姛
     */
    bool finalizeDownload(DownloadContext& context);

    /**
     * @brief 娓呯悊涓嬭浇涓婁笅鏂?
     * @param taskId 浠诲姟ID
     * @param removeFile 鏄惁鍒犻櫎涓存椂鏂囦欢
     */
    void cleanupContext(QString taskId, bool removeFile = true);

    /**
     * @brief 鑾峰彇涓嬭浇涓婁笅鏂?
     * @param taskId 浠诲姟ID
     * @return 涓婁笅鏂囨寚閽堬紙鍙兘涓簄ullptr锛?
     */
    DownloadContext* getContext(QString taskId);

private:
    ServerConnector* m_serverConnector;  // 鏈嶅姟鍣ㄨ繛鎺ュ櫒锛堜笉璐熻矗鎵€鏈夋潈锛?
    LocalDAO* m_dao;                      // 鏁版嵁搴撹闂璞★紙涓嶈礋璐ｆ墍鏈夋潈锛?
    QMap<QString, DownloadContext*> m_downloads;  // 娲诲姩涓嬭浇浠诲姟
    QTimer* m_autoSaveTimer;              // 鑷姩淇濆瓨瀹氭椂鍣?

    bool m_initialized;
};

#endif // FILETRANSFERMANAGER_H
