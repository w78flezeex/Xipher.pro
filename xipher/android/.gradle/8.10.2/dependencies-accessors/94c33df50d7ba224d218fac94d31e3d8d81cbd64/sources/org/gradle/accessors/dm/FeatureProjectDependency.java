package org.gradle.accessors.dm;

import org.gradle.api.NonNullApi;
import org.gradle.api.artifacts.ProjectDependency;
import org.gradle.api.internal.artifacts.dependencies.ProjectDependencyInternal;
import org.gradle.api.internal.artifacts.DefaultProjectDependencyFactory;
import org.gradle.api.internal.artifacts.dsl.dependencies.ProjectFinder;
import org.gradle.api.internal.catalog.DelegatingProjectDependency;
import org.gradle.api.internal.catalog.TypeSafeProjectDependencyFactory;
import javax.inject.Inject;

@NonNullApi
public class FeatureProjectDependency extends DelegatingProjectDependency {

    @Inject
    public FeatureProjectDependency(TypeSafeProjectDependencyFactory factory, ProjectDependencyInternal delegate) {
        super(factory, delegate);
    }

    /**
     * Creates a project dependency on the project at path ":feature:admin"
     */
    public Feature_AdminProjectDependency getAdmin() { return new Feature_AdminProjectDependency(getFactory(), create(":feature:admin")); }

    /**
     * Creates a project dependency on the project at path ":feature:auth"
     */
    public Feature_AuthProjectDependency getAuth() { return new Feature_AuthProjectDependency(getFactory(), create(":feature:auth")); }

    /**
     * Creates a project dependency on the project at path ":feature:bots"
     */
    public Feature_BotsProjectDependency getBots() { return new Feature_BotsProjectDependency(getFactory(), create(":feature:bots")); }

    /**
     * Creates a project dependency on the project at path ":feature:calls"
     */
    public Feature_CallsProjectDependency getCalls() { return new Feature_CallsProjectDependency(getFactory(), create(":feature:calls")); }

    /**
     * Creates a project dependency on the project at path ":feature:channels"
     */
    public Feature_ChannelsProjectDependency getChannels() { return new Feature_ChannelsProjectDependency(getFactory(), create(":feature:channels")); }

    /**
     * Creates a project dependency on the project at path ":feature:chat"
     */
    public Feature_ChatProjectDependency getChat() { return new Feature_ChatProjectDependency(getFactory(), create(":feature:chat")); }

    /**
     * Creates a project dependency on the project at path ":feature:chats"
     */
    public Feature_ChatsProjectDependency getChats() { return new Feature_ChatsProjectDependency(getFactory(), create(":feature:chats")); }

    /**
     * Creates a project dependency on the project at path ":feature:groups"
     */
    public Feature_GroupsProjectDependency getGroups() { return new Feature_GroupsProjectDependency(getFactory(), create(":feature:groups")); }

    /**
     * Creates a project dependency on the project at path ":feature:settings"
     */
    public Feature_SettingsProjectDependency getSettings() { return new Feature_SettingsProjectDependency(getFactory(), create(":feature:settings")); }

}
