struct slentry
{
  /* Port: expanded state for groups, port for servers */
   int port;
   char channel[202];
   char server[132];
   char password[86];
   char comment[100];
   gboolean autoconnect;
};

struct defaultserv
{
   char *channel;
   char *server;
   char *comment;
   int port;
   gboolean autoconnect;
};

struct defaultserv dserv[] =
{
   {"", "SUB", "ChatJunkiesNet", 1, 0},
   {"#linux", "irc.xchat.org", "X-Chat IRC #linux", 6667, 1},
   {"#linux", "us.xchat.org", "X-Chat US #linux", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "IRCNet", 0, 0},
   {"", "irc.stealth.net", "irc.stealth.net", 6667, 0},
   {"", "irc.funet.fi", "irc.funet.fi", 6667, 0},
   {"", "flute.telstra.net.au", "flute.telstra.net.au", 6667, 0},
   {"", "irc.uni-erlangen.de", "irc.uni-erlangen.de", 6667, 0},
   {"", "irc.leo.org", "irc.leo.org", 6667, 0},
   {"", "irc.uni-stuttgart.de", "irc.uni-stuttgart.de", 6667, 0},
   {"", "irc.webbernet.net", "irc.webbernet.net", 6667, 0},
   {"", "irc.fu-berlin.de", "irc.fu-berlin.de", 6667, 0},
   {"", "irc.gmd.de", "irc.gmd.de", 6667, 0},
   {"", "irc.netsurf.de", "irc.netsurf.de", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "DALNet", 0, 0},
   {"", "irc.dal.net", "irc.dal.net", 6667, 0},
   {"", "stlouis.mo.us.dal.net", "stlouis.mo.us.dal.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "EFNet", 0, 0},
   {"", "irc.efnet.net", "irc.efnet.net", 6667, 0},
   {"", "irc.ais.net", "irc.ais.net", 6667, 0},
   {"", "irc.phoenix.net", "irc.phoenix.net", 6667, 0},
   {"", "irc.primenet.com", "irc.primenet.com", 6667, 0},
   {"", "efnet.telstra.net.au", "efnet.telstra.net.au", 6667, 0},
   {"", "irc.lightning.net", "irc.lightning.net", 6667, 0},
   {"", "irc.mindspring.com", "irc.mindspring.com", 6667, 0},
   {"", "irc.core.com", "irc.core.com", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "GalaxyNet", 0, 0},
   {"", "sprynet.us.galaxynet.org", "sprynet.us.galaxynet.org", 6667, 0},
   {"", "atlanta.ga.us.galaxynet.org", "atlanta.ga.us.galaxynet.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "UnderNet", 0, 0},
   {"", "us.undernet.org", "UnderNet US", 6667, 0},
   {"", "eu.undernet.org", "UnderNet EU", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "OpenProjectsNet", 0, 0},
   {"", "irc.openprojects.net", "irc.openprojects.net", 6667, 0},
   {"", "irc.linux.com", "irc.linux.com", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "OtherNet", 0, 0},
   {"", "irc.othernet.org", "irc.othernet.org", 6667, 0},
   {"", "LosAngeles.CA.US.Othernet.Org", "LosAngeles.CA.US.Othernet.Org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "AustNet", 0, 0},
   {"", "us.austnet.org", "AustNet US", 6667, 0},
   {"", "ca.austnet.org", "AustNet CA", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "OzNet", 0, 0},
   {"", "sydney.oz.org", "sydney.oz.org", 6667, 0},
   {"", "melbourne.oz.org", "melbourne.oz.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "FoxChat", 0, 0},
   {"", "irc.FoxChat.net", "irc.FoxChat.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "IrcLink", 0, 0},
   {"", "irc.irclink.net", "irc.irclink.net", 6667, 0},
   {"", "Alesund.no.eu.irclink.net", "Alesund.no.eu.irclink.net", 6667, 0},
   {"", "Oslo.no.eu.irclink.net", "Oslo.no.eu.irclink.net", 6667, 0},
   {"", "Oslo-r.no.eu.irclink.net", "Oslo-r.no.eu.irclink.net", 6667, 0},
   {"", "frogn.no.eu.irclink.net", "frogn.no.eu.irclink.net", 6667, 0},
   {"", "tonsberg.no.eu.irclink.net", "tonsberg.no.eu.irclink.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "XWorld", 0, 0},
   {"#chatzone", "Minneapolis.MN.US.Xworld.Org", "Minneapolis.MN.US.Xworld.Org", 6667, 0},
   {"#chatzone", "STLouis.MO.US.XWorld.Org", "STLouis.MO.US.XWorld.Org", 6667, 0},
   {"#chatzone", "Buffalo.NY.US.XWorld.org", "Buffalo.NY.US.XWorld.org", 6667, 0},
   {"#chatzone", "Oslo.NO.EU.XWorld.org", "Oslo.NO.EU.XWorld.org", 6667, 0},
   {"#chatzone", "Bayern.DE.EU.XWorld.Org", "Bayern.DE.EU.XWorld.Org", 6667, 0},
   {"#chatzone", "Berkeley.CA.US.XWorld.Org", "Berkeley.CA.US.XWorld.Org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "ChatNet", 0, 0},
   {"", "US.ChatNet.Org", "ChatNet USA", 6667, 0},
   {"", "EU.ChatNet.Org", "ChatNet Europe", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "AnyNet", 0, 0},
   {"#anynet", "irc.anynet.org", "irc.anynet.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "KewlNet", 0, 0},
   {"", "irc.kewl.org", "irc.kewl.org", 6667, 0},
   {"", "sheffield.uk.eu.kewl.org", "sheffield.uk.eu.kewl.org", 6667, 0},
   {"", "la.defense.fr.eu.kewl.org", "la.defense.fr.eu.kewl.org", 6667, 0},
   {"", "nanterre.fr.eu.kewl.org", "nanterre.fr.eu.kewl.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "MagicStar (Gaming network)", 0, 0},
   {"", "irc.magicstar.net", "irc.magicstar.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "AsixNet", 0, 0},
   {"", "us.asixnet.org", "us.asixnet.org", 6667, 0},
   {"", "eu.asixnet.org", "eu.asixnet.org", 6667, 0},
   {"", "au.asixnet.org", "au.asixnet.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "DraconicNet", 0, 0},
   {"", "irc.draconic.com", "irc.draconic.com", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "edu.ar", 0, 0},
   {"#linux", "galadriel.ubp.edu.ar", "galadriel.ubp.edu.ar", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "SceneNet", 0, 0},
   {"", "irc.scene.org", "irc.scene.org", 6667, 0},
   {"", "irc.eu.scene.org", "irc.eu.scene.org", 6667, 0},
   {"", "irc.us.scene.org", "irc.us.scene.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "StarChat", 0, 0},
   {"", "irc.starchat.net", "Random StarChat Server", 6667, 0},
   {"", "galatea.starchat.net", "Washington, US", 6667, 0},
   {"", "stargate.starchat.net", "Florida, US", 6667, 0},
   {"", "powerzone.starchat.net", "Texas, US", 6667, 0},
   {"", "utopia.starchat.net", "United Kingdom, EU", 6667, 0},
   {"", "cairns.starchat.net", "Queensland, AU", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "Infinity-IRC.org", 0, 0},
   {"#Linux", "Babylon.CA.US.Infinity-IRC.Org", "Babylon CA #Linux", 6667, 0},
   {"#Linux", "StLouis.MO.US.Infinity-IRC.Org", "St. Louis MO #Linux", 6667, 0},
   {"#Linux", "StarGate.MD.US.Infinity-IRC.Org", "Stargate MD #Linux", 6667, 0},
   {"#Infinity", "Atlanta.GA.US.Infinity-IRC.Org", "Atlanta GA #Infinity", 6667, 0},
   {"#Infinity", "FT-Worth.TX.US.Infinity-IRC.Org", "Ft. Worth TX #Infinity", 6667, 0},
   {"#Linux", "Babylon.DC.US.Infinity-IRC.Org", "Babylon DC #Linux", 6667, 0},
   {"#Infinity", "Dallas.TX.US.Infinity-IRC.Org", "Dallas TX #Infinity", 6667, 0},
   {"#EvilSpeak", "EvilSpeak.NC.US.Infinity-IRC.org", "EvilSpeak NC #EvilSpeak", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "EUIrc", 0, 0},
   {"", "irc.hes.de.euirc.net", "irc.hes.de.euirc.net", 6667, 0},
   {"", "irc.ber.de.euirc.net", "irc.ber.de.euirc.net", 6667, 0},
   {"", "irc.bay.de.euirc.net", "irc.bay.de.euirc.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "WreckedNet", 0, 0},
   {"", "irc.wrecked.net", "Random WreckedNet Server", 6667, 0},
   {"", "sdm.wrecked.net", "British Columbia, CA", 6667, 0},
   {"", "ricochet.wrecked.net", "California, US", 6667, 0},
   {"", "numb.wrecked.net", "California, US", 6667, 0},
   {"", "rockbottom.wrecked.net", "New Jersey, US", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "Mellorien", 0, 0},
   {"", "Irc.mellorien.net", "Irc.mellorien.net", 6667, 0},
   {"", "us.mellorien.net", "us.mellorien.net", 6667, 0},
   {"", "eu.mellorien.net", "eu.mellorien.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "FEFNet", 0, 0},
   {"", "irc.fef.net", "irc.fef.net", 6667, 0},
   {"", "irc.villagenet.com", "irc.villagenet.com", 6667, 0},
   {"", "irc.ggn.net", "irc.ggn.net", 6667, 0},
   {"", "irc.vendetta.com", "irc.vendetta.com", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "Gamma Force", 0, 0},
   {"", "irc.gammaforce.org", "Random Gamma Force server", 6667, 0},
   {"", "sphinx.or.us.gammaforce.org", "Sphinx: US, Orgeon", 6667, 0},
   {"", "monolith.ok.us.gammaforce.org", "Monolith: US, Oklahoma", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "KrushNet.Org", 0, 0},
   {"", "Jeffersonville.IN.US.KrushNet.Org", "Jeffersonville.IN.US.KrushNet.Org", 6667, 0},
   {"", "Auckland.NZ.KrushNet.Org", "Auckland.NZ.KrushNet.Org", 6667, 0},
   {"", "Hastings.NZ.KrushNet.Org", "Hastings.NZ.KrushNet.Org", 6667, 0},
   {"", "Seattle-R.WA.US.KrushNet.Org", "Seattle-R.WA.US.KrushNet.Org", 6667, 0},
   {"", "Minneapolis.MN.US.KrushNet.Org", "Minneapolis.MN.US.KrushNet.Org", 6667, 0},
   {"", "Cullowhee.NC.US.KrushNet.Org", "Cullowhee.NC.US.KrushNet.Org", 6667, 0},
   {"", "Asheville-R.NC.US.KrushNet.Org", "Asheville-R.NC.US.KrushNet.Org", 6667, 0},
   {"", "San-Antonio.TX.US.KrushNet.Org", "San-Antonio.TX.US.KrushNet.Org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "Neohorizon", 0, 0},
   {"", "irc.nhn.net", "irc.nhn.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "TrekLink", 0, 0},
   {"", "neutron.treklink.net", "neutron.treklink.net", 6667, 0},
   {"", "irc.treklink.net", "irc.treklink.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "Librenet", 0, 0},
   {"", "irc.librenet.net", "irc.librenet.net", 6667, 0},
   {"", "eden.be.librenet.net", "eden.be.librenet.net", 6667, 0},
   {"", "Saturn.FR.Librenet.net", "Saturn.FR.Librenet.net", 6667, 0},
   {"", "famipow.fr.librenet.net", "famipow.fr.librenet.net", 6667, 0},
   {"", "ielf.fr.librenet.net", "ielf.fr.librenet.net", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "PTNet, UNI", 0, 0},
   {"", "irc.PTNet.org", "irc.PTNet.org", 6667, 0},
   {"", "rccn.PTnet.org", "rccn.PTnet.org", 6667, 0},
   {"", "uevora.PTnet.org", "uevora.PTnet.org", 6667, 0},
   {"", "umoderna.PTnet.org", "umoderna.PTnet.org", 6667, 0},
   {"", "ist.PTnet.org", "ist.PTnet.org", 6667, 0},
   {"", "aaum.PTnet.org", "aaum.PTnet.org", 6667, 0},
   {"", "uc.PTnet.org", "uc.PTnet.org", 6667, 0},
   {"", "ualg.ptnet.org", "ualg.ptnet.org", 6667, 0},
   {"", "madinfo.PTnet.org", "madinfo.PTnet.org", 6667, 0},
   {"", "isep.PTnet.org", "isep.PTnet.org", 6667, 0},
   {"", "ua.PTnet.org", "ua.PTnet.org", 6667, 0},
   {"", "ipg.PTnet.org", "ipg.PTnet.org", 6667, 0},
   {"", "isec.PTnet.org", "isec.PTnet.org", 6667, 0},
   {"", "utad.PTnet.org", "utad.PTnet.org", 6667, 0},
   {"", "iscte.PTnet.org", "iscte.PTnet.org", 6667, 0},
   {"", "ubi.PTnet.org", "ubi.PTnet.org", 6667, 0},
   {"", "ENDSUB", "", 0, 0},

   {"", "SUB", "PTNet, ISP's", 0, 0},
   {"", "irc.PTNet.org", "irc.PTNet.org", 6667, 0},
   {"", "rccn.PTnet.org", "rccn.PTnet.org", 6667, 0},
   {"", "EUnet.PTnet.org", "EUnet.PTnet.org", 6667, 0},
   {"", "madinfo.PTnet.org", "madinfo.PTnet.org", 6667, 0},
   {"", "netc2.PTnet.org", "netc2.PTnet.org", 6667, 0},
   {"", "netc1.PTnet.org", "netc1.PTnet.org", 6667, 0},
   {"", "teleweb.PTnet.org", "teleweb.PTnet.org", 6667, 0},
   {"", "netway.PTnet.org", "netway.PTnet.org", 6667, 0},
   {"", "telepac1.ptnet.org", "telepac1.ptnet.org", 6667, 0},
   {"", "services.ptnet.org", "services.ptnet.org", 6667, 0},
   {"", "esoterica.PTnet.org", "esoterica.PTnet.org", 6667, 0},
   {"", "ip-hub.ptnet.org", "ip-hub.ptnet.org", 6667, 0},
   {"", "telepac1.ptnet.org", "telepac1.ptnet.org", 6667, 0},
   {"", "nortenet.PTnet.org", "nortenet.PTnet.org", 6667, 0}, 
   {"", "ENDSUB", "", 0, 0},

   {0, 0, 0, 0, 0}
};
