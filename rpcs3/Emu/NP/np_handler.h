#pragma once

#include <queue>
#include <map>
#include <unordered_map>

#include "Emu/Memory/vm_ptr.h"
#include "Emu/Cell/Modules/sceNp.h"
#include "Emu/Cell/Modules/sceNp2.h"

#include "Emu/NP/rpcn_client.h"
#include "generated/np2_structs_generated.h"
#include "signaling_handler.h"
#include "np_event_data.h"
#include "np_allocator.h"

namespace np
{
	struct basic_event
	{
		s32 event = 0;
		SceNpUserInfo from{};
		std::vector<u8> data;
	};

	// Helper functions
	std::string ip_to_string(u32 addr);
	std::string ether_to_string(std::array<u8, 6>& ether);

	void string_to_npid(const std::string& str, SceNpId* npid);
	void string_to_online_name(const std::string& str, SceNpOnlineName* online_name);
	void string_to_avatar_url(const std::string& str, SceNpAvatarUrl* avatar_url);
	void string_to_communication_id(const std::string& str, SceNpCommunicationId* comm_id);

	class np_handler
	{
	public:
		np_handler();

		const std::array<u8, 6>& get_ether_addr() const;
		const std::string& get_hostname() const;
		u32 get_local_ip_addr() const;
		u32 get_public_ip_addr() const;
		u32 get_dns_ip() const;

		s32 get_psn_status() const;
		s32 get_net_status() const;

		const SceNpId& get_npid() const;
		const SceNpOnlineId& get_online_id() const;
		const SceNpOnlineName& get_online_name() const;
		const SceNpAvatarUrl& get_avatar_url() const;

		// DNS hooking functions
		void add_dns_spy(u32 sock);
		void remove_dns_spy(u32 sock);
		bool is_dns(u32 sock) const;
		bool is_dns_queue(u32 sock) const;
		std::vector<u8> get_dns_packet(u32 sock);
		s32 analyze_dns_packet(s32 s, const u8* buf, u32 len);

		// handles async messages from server(only needed for RPCN)
		void operator()();

		void init_NP(u32 poolsize, vm::ptr<void> poolptr);
		void terminate_NP();

		bool is_netctl_init     = false;
		bool is_NP_init         = false;
		bool is_NP_Lookup_init  = false;
		bool is_NP_Score_init   = false;
		bool is_NP2_init        = false;
		bool is_NP2_Match2_init = false;
		bool is_NP_Auth_init    = false;

		// NP Handlers/Callbacks
		// Seems to be global
		vm::ptr<SceNpManagerCallback> manager_cb{}; // Connection status and tickets
		vm::ptr<void> manager_cb_arg{};

		// Basic event handler;
		struct
		{
			SceNpCommunicationId context{};
			vm::ptr<SceNpBasicEventHandler> handler_func;
			vm::ptr<void> handler_arg;
			bool registered        = false;
			bool context_sensitive = false;
		} basic_handler;

		void queue_basic_event(basic_event to_queue);
		bool send_basic_event(s32 event, s32 retCode, u32 reqId);
		error_code get_basic_event(vm::ptr<s32> event, vm::ptr<SceNpUserInfo> from, vm::ptr<s32> data, vm::ptr<u32> size);
		std::optional<std::shared_ptr<std::pair<std::string, message_data>>> get_message(u64 id);

		// Those should probably be under match2 ctx
		vm::ptr<SceNpMatching2RoomEventCallback> room_event_cb{}; // Room events
		u16 room_event_cb_ctx = 0;
		vm::ptr<void> room_event_cb_arg{};
		vm::ptr<SceNpMatching2RoomMessageCallback> room_msg_cb{};
		u16 room_msg_cb_ctx = 0;
		vm::ptr<void> room_msg_cb_arg{};

		// Synchronous requests
		std::vector<SceNpMatching2ServerId> get_match2_server_list(SceNpMatching2ContextId);
		// Asynchronous requests
		u32 get_server_status(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, u16 server_id);
		u32 create_server_context(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, u16 server_id);
		u32 get_world_list(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, u16 server_id);
		u32 create_join_room(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2CreateJoinRoomRequest* req);
		u32 join_room(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2JoinRoomRequest* req);
		u32 leave_room(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2LeaveRoomRequest* req);
		u32 search_room(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SearchRoomRequest* req);
		u32 get_roomdata_external_list(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2GetRoomDataExternalListRequest* req);
		u32 set_roomdata_external(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SetRoomDataExternalRequest* req);
		u32 get_roomdata_internal(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2GetRoomDataInternalRequest* req);
		u32 set_roomdata_internal(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SetRoomDataInternalRequest* req);
		u32 set_roommemberdata_internal(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SetRoomMemberDataInternalRequest* req);
		u32 get_ping_info(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SignalingGetPingInfoRequest* req);
		u32 send_room_message(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam, const SceNpMatching2SendRoomMessageRequest* req);

		u32 get_match2_event(SceNpMatching2EventKey event_key, u32 dest_addr, u32 size);

		// Friend stuff
		u32 get_num_friends();
		u32 get_num_blocks();

		// Misc stuff
		void req_ticket(u32 version, const SceNpId* npid, const char* service_id, const u8* cookie, u32 cookie_size, const char* entitlement_id, u32 consumed_count);
		const std::vector<u8>& get_ticket() const
		{
			return current_ticket;
		}
		u32 add_players_to_history(vm::cptr<SceNpId> npids, u32 count);

		// For signaling
		void req_sign_infos(const std::string& npid, u32 conn_id);

		// Mutex for NP status change
		shared_mutex mutex_status;

		static constexpr std::string_view thread_name = "NP Handler Thread";

	private:
		// Various generic helpers
		void discover_ip_address();
		bool discover_ether_address();
		bool error_and_disconnect(const std::string& error_msg);

		// Notification handlers
		void notif_user_joined_room(std::vector<u8>& data);
		void notif_user_left_room(std::vector<u8>& data);
		void notif_room_destroyed(std::vector<u8>& data);
		void notif_updated_room_data_internal(std::vector<u8>& data);
		void notif_updated_room_member_data_internal(std::vector<u8>& data);
		void notif_p2p_connect(std::vector<u8>& data);
		void notif_room_message_received(std::vector<u8>& data);

		// Reply handlers
		bool reply_get_world_list(u32 req_id, std::vector<u8>& reply_data);
		bool reply_create_join_room(u32 req_id, std::vector<u8>& reply_data);
		bool reply_join_room(u32 req_id, std::vector<u8>& reply_data);
		bool reply_leave_room(u32 req_id, std::vector<u8>& reply_data);
		bool reply_search_room(u32 req_id, std::vector<u8>& reply_data);
		bool reply_get_roomdata_external_list(u32 req_id, std::vector<u8>& reply_data);
		bool reply_set_roomdata_external(u32 req_id, std::vector<u8>& reply_data);
		bool reply_get_roomdata_internal(u32 req_id, std::vector<u8>& reply_data);
		bool reply_set_roomdata_internal(u32 req_id, std::vector<u8>& reply_data);
		bool reply_set_roommemberdata_internal(u32 req_id, std::vector<u8>& reply_data);
		bool reply_get_ping_info(u32 req_id, std::vector<u8>& reply_data);
		bool reply_send_room_message(u32 req_id, std::vector<u8>& reply_data);
		bool reply_req_sign_infos(u32 req_id, std::vector<u8>& reply_data);
		bool reply_req_ticket(u32 req_id, std::vector<u8>& reply_data);

		// Helper functions(fb=>np2)
		void BinAttr_to_SceNpMatching2BinAttr(event_data& edata, const BinAttr* bin_attr, SceNpMatching2BinAttr* binattr_info);
		void BinAttrs_to_SceNpMatching2BinAttrs(event_data& edata, const flatbuffers::Vector<flatbuffers::Offset<BinAttr>>* fb_attr, SceNpMatching2BinAttr* binattr_info);
		void RoomMemberBinAttrInternal_to_SceNpMatching2RoomMemberBinAttrInternal(event_data& edata, const RoomMemberBinAttrInternal* fb_attr, SceNpMatching2RoomMemberBinAttrInternal* binattr_info);
		void RoomBinAttrInternal_to_SceNpMatching2RoomBinAttrInternal(event_data& edata, const BinAttrInternal* fb_attr, SceNpMatching2RoomBinAttrInternal* binattr_info);
		void RoomGroup_to_SceNpMatching2RoomGroup(const RoomGroup* fb_group, SceNpMatching2RoomGroup* sce_group);
		void RoomGroups_to_SceNpMatching2RoomGroups(const flatbuffers::Vector<flatbuffers::Offset<RoomGroup>>* fb_groups, SceNpMatching2RoomGroup* sce_groups);
		void UserInfo2_to_SceNpUserInfo2(event_data& edata, const UserInfo2* user, SceNpUserInfo2* user_info);
		void RoomDataExternal_to_SceNpMatching2RoomDataExternal(event_data& edata, const RoomDataExternal* room, SceNpMatching2RoomDataExternal* room_info);
		void SearchRoomResponse_to_SceNpMatching2SearchRoomResponse(event_data& edata, const SearchRoomResponse* resp, SceNpMatching2SearchRoomResponse* search_resp);
		void GetRoomDataExternalListResponse_to_SceNpMatching2GetRoomDataExternalListResponse(event_data& edata, const GetRoomDataExternalListResponse* resp, SceNpMatching2GetRoomDataExternalListResponse* get_resp);
		u16 RoomDataInternal_to_SceNpMatching2RoomDataInternal(event_data& edata, const RoomDataInternal* resp, SceNpMatching2RoomDataInternal* room_resp, const SceNpId& npid);
		void RoomMemberDataInternal_to_SceNpMatching2RoomMemberDataInternal(event_data& edata, const RoomMemberDataInternal* member_data, const SceNpMatching2RoomDataInternal* room_info, SceNpMatching2RoomMemberDataInternal* sce_member_data);
		void RoomMemberUpdateInfo_to_SceNpMatching2RoomMemberUpdateInfo(event_data& edata, const RoomMemberUpdateInfo* resp, SceNpMatching2RoomMemberUpdateInfo* room_info);
		void RoomUpdateInfo_to_SceNpMatching2RoomUpdateInfo(const RoomUpdateInfo* update_info, SceNpMatching2RoomUpdateInfo* sce_update_info);
		void GetPingInfoResponse_to_SceNpMatching2SignalingGetPingInfoResponse(const GetPingInfoResponse* resp, SceNpMatching2SignalingGetPingInfoResponse* sce_resp);
		void RoomMessageInfo_to_SceNpMatching2RoomMessageInfo(event_data& edata, const RoomMessageInfo* mi, SceNpMatching2RoomMessageInfo* sce_mi);
		void RoomDataInternalUpdateInfo_to_SceNpMatching2RoomDataInternalUpdateInfo(event_data& edata, const RoomDataInternalUpdateInfo* update_info, SceNpMatching2RoomDataInternalUpdateInfo* sce_update_info, const SceNpId& npid);
		void RoomMemberDataInternalUpdateInfo_to_SceNpMatching2RoomMemberDataInternalUpdateInfo(event_data& edata, const RoomMemberDataInternalUpdateInfo* update_info, SceNpMatching2RoomMemberDataInternalUpdateInfo* sce_update_info);

		struct callback_info
		{
			SceNpMatching2ContextId ctx_id;
			vm::ptr<SceNpMatching2RequestCallback> cb;
			vm::ptr<void> cb_arg;
		};
		u32 generate_callback_info(SceNpMatching2ContextId ctx_id, vm::cptr<SceNpMatching2RequestOptParam> optParam);
		callback_info take_pending_request(u32 req_id);

		shared_mutex mutex_pending_requests;
		std::unordered_map<u32, callback_info> pending_requests;
		shared_mutex mutex_pending_sign_infos_requests;
		std::unordered_map<u32, u32> pending_sign_infos_requests;

		shared_mutex mutex_queue_basic_events;
		std::queue<basic_event> queue_basic_events;

	private:
		bool is_connected  = false;
		bool is_psn_active = false;

		std::vector<u8> current_ticket;

		// IP & DNS info
		std::string hostname = "localhost";
		std::array<u8, 6> ether_address{};
		be_t<u32> local_ip_addr{};
		be_t<u32> public_ip_addr{};
		be_t<u32> dns_ip = 0x08080808;

		// User infos
		SceNpId npid{};
		SceNpOnlineName online_name{};
		SceNpAvatarUrl avatar_url{};

		// DNS related
		std::map<s32, std::queue<std::vector<u8>>> dns_spylist{};
		std::map<std::string, u32> switch_map{};

		// Memory pool for sceNp/sceNp2
		memory_allocator np_memory;

		// Requests(reqEventKey : data)
		shared_mutex mutex_match2_req_results;
		std::unordered_map<u32, event_data> match2_req_results;
		atomic_t<u16> match2_low_reqid_cnt = 1;
		atomic_t<u32> match2_event_cnt     = 1;
		u32 get_req_id(u16 app_req)
		{
			return ((app_req << 16) | match2_low_reqid_cnt.fetch_add(1));
		}
		u32 get_event_key()
		{
			return match2_event_cnt.fetch_add(1);
		}
		event_data& allocate_req_result(u32 event_key, u32 max_size, u32 initial_size);

		// RPCN
		shared_mutex mutex_rpcn;
		std::shared_ptr<rpcn::rpcn_client> rpcn;
	};
} // namespace np
