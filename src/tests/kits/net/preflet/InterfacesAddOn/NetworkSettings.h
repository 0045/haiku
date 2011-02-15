/*
 * Copyright 2004-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Andre Alves Garzia, andre@andregarzia.com
 *		Vegard Wærp, vegarwa@online.no
 *		Alexander von Gluck, kallisti5@unixzen.com
 */
#ifndef SETTINGS_H
#define SETTINGS_H


#include <NetworkDevice.h>
#include <NetworkInterface.h>
#include <ObjectList.h>
#include <String.h>


class NetworkSettings {
public:
								NetworkSettings(const char* name);
	virtual						~NetworkSettings();

//			void				SetIP(const BString& ip)
//									{ fIP = ip; }
//			void				SetGateway(const BString& ip)
//									{ fGateway = ip; }
//			void				SetNetmask(const BString& ip)
//									{ fNetmask = ip; }
//			void				SetDomain(const BString& domain)
//									{ fDomain = domain; }
//			void				SetAutoConfigure(bool autoConfigure)
//									{ fAuto = autoConfigure; }
			void				SetDisabled(bool disabled)
									{ fDisabled = disabled; }
//			void				SetWirelessNetwork(const char* name)
//									{ fWirelessNetwork.SetTo(name); }

			BNetworkAddress		IPAddr(int family);

			const char*			IP(int family);
			const char*			Netmask(int family);
			const char*			Gateway(int family);
			int32				PrefixLen(int family);

			const char*			Name()  { return fName.String(); }
			const char*			Domain() { return fDomain.String(); }
			bool				AutoConfigure() { return fIPv4Auto; }
			bool				IsDisabled() { return fDisabled; }
			const BString&		WirelessNetwork() { return fWirelessNetwork; }

			BObjectList<BString>& NameServers() { return fNameServers; }

			void				ReadConfiguration();

private:
			int					fSocket4;
			int					fSocket6;

			BNetworkInterface	fNetworkInterface;

			// IPv4 address configuration
			bool				fIPv4Auto;
			BNetworkAddress		fIPv4Addr;
			BNetworkAddress		fIPv4Mask;
			BNetworkAddress		fIPv4Gateway;

			// IPv6 address configuration
			bool				fIPv6Auto;
			BNetworkAddress		fIPv6Addr;
			BNetworkAddress		fIPv6Mask;
			BNetworkAddress		fIPv6Gateway;

			BString				fName;
			BString				fDomain;
			bool				fDisabled;
			BObjectList<BString> fNameServers;
			BString				fWirelessNetwork;
};


#endif /* SETTINGS_H */
